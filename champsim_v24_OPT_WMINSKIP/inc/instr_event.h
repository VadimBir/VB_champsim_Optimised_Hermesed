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
    // WMIN-SKIP: per-word lower bound on event_cycle of in-range entries.
    // Only ever LOWERED at event_cycle lowering write sites; never raised here.
    // A loose (stale-low) value only DISABLES the skip -> always sound.
    uint64_t wmin[ROB_WORDS];

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
