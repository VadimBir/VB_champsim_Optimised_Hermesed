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

// HEAP-SCHED steelman: user's "heap of only-schedulable entries" idea, made
// bit-exact. The ONLY time-varying quantity in the fused scan is
//   cr[w] = (event_cycle[idx] <= now)   for idx in word w.
// Both ready = (fc & ~sinf & ~scmp) & cr  and  stall = ~(fc & cr) need cr.
// So the heap's job is to maintain cr_bitset[8] == "event_cycle <= now" mask
// WITHOUT the per-word 64-iteration PCYCLE_LE inner loop. We use a lazy
// binary min-heap keyed by event_cycle holding (key, idx) for every entry
// whose event_cycle is still in the FUTURE (event_cycle > now). Each cycle we
// pop all roots with key <= now and, after a lazy-stale re-check against the
// live event_cycle[idx], set the corresponding cr_bitset bit. Entries written
// with event_cycle <= now set their bit immediately (and are NOT heaped).
// An entry whose event_cycle is rewritten into the future (e.g. do_scheduling
// non-mem bump) has its cr bit cleared and is (re)pushed. Stale heap nodes
// (whose live event_cycle no longer matches the node key) are skipped on pop.
struct HeapNode { uint64_t key; uint16_t idx; };

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

    // (A) TIMING min-structure: cr_bitset + lazy min-heap keyed by event_cycle.
    uint64_t cr_bitset[ROB_WORDS];          // bit idx == (event_cycle[idx] <= now)
    HeapNode heap[ROB_SIZE * 16];           // lazy heap (stale nodes accumulate)
    uint32_t heap_size;

    inline void heap_clear() noexcept { heap_size = 0; }

    inline void heap_push(uint64_t key, uint16_t idx) noexcept {
        assert(heap_size < ROB_SIZE * 16);
        uint32_t i = heap_size++;
        heap[i] = {key, idx};
        while (i) {
            uint32_t p = (i - 1) >> 1;
            if (heap[p].key <= heap[i].key) break;
            HeapNode t = heap[p]; heap[p] = heap[i]; heap[i] = t;
            i = p;
        }
    }

    inline void heap_pop() noexcept {
        heap[0] = heap[--heap_size];
        uint32_t i = 0;
        for (;;) {
            uint32_t l = 2 * i + 1, r = l + 1, m = i;
            if (l < heap_size && heap[l].key < heap[m].key) m = l;
            if (r < heap_size && heap[r].key < heap[m].key) m = r;
            if (m == i) break;
            HeapNode t = heap[m]; heap[m] = heap[i]; heap[i] = t;
            i = m;
        }
    }

    // EC_WRITE: single hooked path for every event_cycle[idx] = V site.
    // Maintains cr_bitset/heap invariant for any write direction.
    inline void ec_write(uint16_t idx, uint64_t val, uint64_t now) noexcept {
        event_cycle[idx] = val;
        const uint16_t w = idx >> 6;
        const uint64_t b = 1ULL << (idx & 63);
        if (val <= now) {
            cr_bitset[w] |= b;          // matured immediately
        } else {
            cr_bitset[w] &= ~b;         // pushed into the future
            heap_push(val, idx);        // schedule lazy maturation
        }
    }

    // mature: pop all heap roots with key <= now into cr_bitset (lazy-stale
    // skip: node only matures a bit if its key still matches live event_cycle).
    inline void mature(uint64_t now) noexcept {
        while (heap_size && heap[0].key <= now) {
            const uint16_t idx = heap[0].idx;
            const uint64_t key = heap[0].key;
            heap_pop();
            if (event_cycle[idx] == key && key <= now)
                cr_bitset[idx >> 6] |= (1ULL << (idx & 63));
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
