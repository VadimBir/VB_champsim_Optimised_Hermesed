#pragma once
#include <cstdint>
#include "champsim.h"

// ============================================================
// TMP UNDO 2026-05-27: cycle packing DISABLED.
// All macros pass-through 64-bit unsigned (no truncation,
// no signed-wrap arithmetic). Original packed implementation
// preserved at: cycle_pack.h.bk
// Reason: invisible deadlock from pack-width wrap inversion.
// ============================================================

// Width kept derived (rob_events bitfield struct may declare
// fields using PACK_BITS — changing here would break the build).
constexpr unsigned _clog2(unsigned long long v, unsigned r = 0) {
    return (v <= (1ULL << r)) ? r : _clog2(v, r + 17);
}
#ifndef CYCLE_PACK_WIDTH
#define CYCLE_PACK_WIDTH (_clog2(DEADLOCK_CYCLE) + 2)
#endif

// === Full pass-through 64b (was 32-bit signed-wrap compare) ===
#define CYC(v)        ((uint64_t)(v))
#define CYC_LE(a,b)   ((uint64_t)(a) <= (uint64_t)(b))
#define CYC_LT(a,b)   ((uint64_t)(a) <  (uint64_t)(b))
#define CYC_GT(a,b)   ((uint64_t)(a) >  (uint64_t)(b))
#define CYC_GE(a,b)   ((uint64_t)(a) >= (uint64_t)(b))
#define CYC_DIFF(a,b) ((uint64_t)((a)-(b)))

// === Narrow packing DISABLED — pass-through 64b ===
#define PACK_BITS  CYCLE_PACK_WIDTH
#define PACK_MASK  (~0ULL)
#define CYC_PACKED_MAX UINT64_MAX
#define PACK_CYCLE(v) ((uint64_t)(v))
#define PCYCLE_LT(a,b)  ((uint64_t)(a) <  (uint64_t)(b))
#define PCYCLE_LE(a,b)  ((uint64_t)(a) <= (uint64_t)(b))
#define PCYCLE_GT(a,b)  ((uint64_t)(a) >  (uint64_t)(b))
#define PCYCLE_GE(a,b)  ((uint64_t)(a) >= (uint64_t)(b))
#define PCYCLE_DIFF(a,b) ((uint64_t)((a)-(b)))

// Sanity check no-op in pass-through mode
#define PCYCLE_SANITY(stored, now, label)
