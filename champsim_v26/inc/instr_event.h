#include "defs.h"
#include "champsim.h"
#include "cycle_pack.h"
#include <cstdint>
#include <cstring>
#include <cassert>
#pragma once

static constexpr uint32_t ROB_WORDS = (ROB_SIZE + 63) / 64;  // 8 for ROB_SIZE=512

// Branchless bitset macros — guaranteed zero overhead
#define BS_SET(bs, idx)   ((bs)[(idx) >> 6] |=  (1ULL << ((idx) & 63)))
#define BS_CLR(bs, idx)   ((bs)[(idx) >> 6] &= ~(1ULL << ((idx) & 63)))
#define BS_TST(bs, idx)   (((bs)[(idx) >> 6] >> ((idx) & 63)) & 1)

// NO-HEAP variant of HEAP-SCHED. Same goal: maintain cr_bitset[8] ==
//   cr[w] = (event_cycle[idx] <= now)   for idx in word w
// WITHOUT the per-word 64-iteration PCYCLE_LE inner loop, and WITHOUT a heap
// (no HeapNode array, no sift, no overflow risk). Future-dated entries are
// tracked in future_bitset[ROB_WORDS]: bit i set iff entry i currently has
// event_cycle > now (not yet ready, will mature by clock). Each cycle mature()
// scans only the SET BITS of future_bitset (via __builtin_ctzll) and promotes
// those whose event_cycle has reached <= now into cr_bitset; no ordering is
// needed. cr_bitset holds the identical bits as HEAPSCHED (set iff
// event_cycle <= now) by construction, the dispatch loop is unchanged, so the
// dispatched-index sequence is identical -> IPC bit-exact.

// Per-CPU SOA — all bitsets for one CPU contiguous in memory
struct alignas(64) rob_events_cpu {
    uint64_t is_mem[ROB_WORDS];
    uint64_t reg_ready[ROB_WORDS];
    uint64_t fetched_inflight[ROB_WORDS];
    uint64_t fetched_complete[ROB_WORDS];
    uint64_t sched_inflight[ROB_WORDS];
    uint64_t sched_complete[ROB_WORDS];
    uint64_t exec_inflight[ROB_WORDS];
    uint64_t event_cycle[ROB_SIZE];

    // (A) TIMING structure: cr_bitset + future_bitset (NO heap).
    uint64_t cr_bitset[ROB_WORDS];          // bit idx == (event_cycle[idx] <= now)
    uint64_t future_bitset[ROB_WORDS];      // bit idx == tracked & (event_cycle[idx] > now)

    // EC_WRITE: single hooked path for every event_cycle[idx] = V site.
    // Maintains cr_bitset/future_bitset invariant for any write direction.
    inline void ec_write(uint16_t idx, uint64_t val, uint64_t now) noexcept {
        event_cycle[idx] = val;
        const uint16_t w = idx >> 6;
        const uint64_t b = 1ULL << (idx & 63);
        if (PCYCLE_LE(val, now)) {
            cr_bitset[w]     |=  b;     // matured immediately
            future_bitset[w] &= ~b;     // no longer future-pending
        } else {
            cr_bitset[w]     &= ~b;     // pushed into the future
            future_bitset[w] |=  b;     // track for lazy maturation
        }
    }

    // mature: scan only the SET BITS of future_bitset; promote entries whose
    // event_cycle has reached <= now into cr_bitset (no ordering needed).
    inline void mature(uint64_t now) noexcept {
        for (uint16_t w = 0; w < ROB_WORDS; ++w) {
            uint64_t f = future_bitset[w];
            while (f) {
                const uint32_t bit = __builtin_ctzll(f);
                const uint16_t idx = (uint16_t)((w << 6) + bit);
                if (PCYCLE_LE(event_cycle[idx], now)) {
                    cr_bitset[w]     |=  (1ULL << bit);
                    future_bitset[w] &= ~(1ULL << bit);
                }
                f &= f - 1;             // clear lowest set bit
            }
        }
    }

    inline void clear_entry(uint16_t idx) noexcept {
        const uint64_t mask = ~(1ULL << (idx & 63));
        const uint16_t w = idx >> 6;
        is_mem[w]          &= mask;
        reg_ready[w]       &= mask;
        fetched_inflight[w]&= mask;
        fetched_complete[w]&= mask;
        sched_inflight[w]  &= mask;
        sched_complete[w]  &= mask;
        exec_inflight[w]   &= mask;
        cr_bitset[w]       &= mask;
        future_bitset[w]   &= mask;
        event_cycle[idx] = 0;
    }
};

// Wrapper: rob_events_soa keeps the same external API via [cpu] indexing
struct alignas(64) rob_events_soa {
    rob_events_cpu per_cpu[NUM_CPUS];

    // Proxy accessors — return pointer to per-cpu array so existing code
    // using rob_events.field[cpu] still compiles with minimal changes.
    // These are NOT arrays — they're accessor helpers.

    inline void clear_entry(uint8_t cpu, uint16_t idx) noexcept {
        per_cpu[cpu].clear_entry(idx);
    }
};

// Field accessor macros — translate rob_events.field[cpu] to rob_events.per_cpu[cpu].field
// This avoids touching every callsite.
#define ROB_EVT(field, cpu) (rob_events.per_cpu[cpu].field)

extern rob_events_soa rob_events;

struct alignas(64) MemIndexRing {
    alignas(64) uint16_t idx[NUM_CPUS][ROB_SIZE];
    uint16_t head[NUM_CPUS];
    uint16_t tail[NUM_CPUS];
    uint16_t count[NUM_CPUS];

    void clear(uint8_t cpu) noexcept {
        head[cpu] = tail[cpu] = count[cpu] = 0;
    }

    inline void push(uint8_t cpu, uint16_t rob_idx) noexcept {
        idx[cpu][tail[cpu]] = rob_idx;
        tail[cpu]++;
        if (tail[cpu] == ROB_SIZE) tail[cpu] = 0;
        count[cpu]++;
    }

    inline void pop_head(uint8_t cpu) noexcept {
        head[cpu]++;
        if (head[cpu] == ROB_SIZE) head[cpu] = 0;
        count[cpu]--;
    }

    inline bool empty(uint8_t cpu) const noexcept { return count[cpu] == 0; }
};

extern MemIndexRing mem_index_ring;
