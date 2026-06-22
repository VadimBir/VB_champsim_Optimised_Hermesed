#pragma once
#include <cstdint>
#include "champsim.h"
#include "webrtc_seqnum.h"   // vendored WebRTC AheadOf/AheadOrAt (BSD-3)

// ============================================================
// v25: cycle packing RE-ENABLED — correct, UB-free, deadlock-free.
//
// Old packed compare (cycle_pack.h.bk era) was:
//   PCYCLE_LE(a,b) = (int32_t)(((a)-(b)) << (32-PACK_BITS)) <= 0
// Two defects:
//   1. signed left-shift-into-sign is signed-overflow UB pre-C++20.
//   2. the compare INVERTS once the true gap |a-b| >= 2^(PACK_BITS-1),
//      which at PACK_BITS=27 is 2^26=67,108,864. Master-clock growth +
//      32-bit container truncation/operand-mixing produced the silent
//      4-billion-cycle deadlock.
//
// Fix here: serial-number arithmetic (RFC 1982 / WebRTC
// rtc_base/numerics/sequence_number_util.h AheadOrAt, BSD-3; Linux
// kernel before()). NO left-shift, NO UB. Compare reduces (b-a) to
// exactly W bits and tests against the half-window. Both operands are
// masked to exactly W bits via PACK_CYCLE before any compare.
//
// Safety: DEADLOCK_CYCLE=20,000,000 (champsim.h) < HALF_WINDOW=2^26=
// 67,108,864. Every live cycle gap is bounded by the deadlock horizon,
// so it stays strictly inside the safe window -> compares are bit-exact
// vs a 64-bit oracle.
// ============================================================

#ifndef DO_CYCLE_PACKING
#define DO_CYCLE_PACKING 1 // re-enabled per request
#endif

// clog2(v) = smallest r with v <= 2^r.  (was r+17 typo -> 36; r+1 -> 27)
constexpr unsigned _clog2(unsigned long long v, unsigned r = 0) {
    return (v <= (1ULL << r)) ? r : _clog2(v, r + 1);
}
#ifndef CYCLE_PACK_WIDTH
#define CYCLE_PACK_WIDTH (_clog2(DEADLOCK_CYCLE) + 2)   // = 27
#endif

#define PACK_BITS    CYCLE_PACK_WIDTH                    // 27
#define PACK_MASK    ((1u << PACK_BITS) - 1)             // low-27-bit mask
#define HALF_WINDOW  (1u << (PACK_BITS - 1))             // 2^26 = 67,108,864
#define CYC_PACKED_MAX ((uint64_t)PACK_MASK)

// === Full-width 32-bit serial compare (kernel idiom). UB-free: plain
//     32-bit subtraction wraps mod 2^32 (defined for unsigned), the
//     (int32_t) cast is the natural before() test, no shift. ============
#define CYC(v)        ((uint64_t)(v))
#define CYC_LE(a,b)   ((int32_t)((uint32_t)(a) - (uint32_t)(b)) <= 0)
#define CYC_LT(a,b)   ((int32_t)((uint32_t)(a) - (uint32_t)(b)) <  0)
#define CYC_GT(a,b)   ((int32_t)((uint32_t)(a) - (uint32_t)(b)) >  0)
#define CYC_GE(a,b)   ((int32_t)((uint32_t)(a) - (uint32_t)(b)) >= 0)
#define CYC_DIFF(a,b) ((uint64_t)((uint32_t)(a) - (uint32_t)(b)))

#if DO_CYCLE_PACKING
// ---- W-bit serial-number packed compares, delegated to the vendored
//      WebRTC sequence-number lib (webrtc::AheadOf / AheadOrAt, BSD-3).
//      Modulus M = 2^PACK_BITS = 2^27 = 134,217,728 (even -> deterministic
//      midpoint, no RFC-1982 undefined point, no signed-shift UB).
//      The M>0 templates require both operands < M, so each is masked to
//      exactly W bits before the call.
//
//      Mapping: PCYCLE_LE(a,b) == "a <= b" == "b is ahead-or-at a"
//               -> webrtc::AheadOrAt<uint32_t, M>(b, a)
//               PCYCLE_LT(a,b) == "a <  b" == "b is ahead of a"
//               -> webrtc::AheadOf<uint32_t, M>(b, a)
#define PACK_CYCLE(v)    ((uint32_t)((v) & PACK_MASK))
#define PCYCLE_LE(a,b)   ( webrtc::AheadOrAt<uint32_t, (1u << PACK_BITS)>( \
                              (uint32_t)((b) & PACK_MASK), \
                              (uint32_t)((a) & PACK_MASK)) )
#define PCYCLE_LT(a,b)   ( webrtc::AheadOf<uint32_t, (1u << PACK_BITS)>( \
                              (uint32_t)((b) & PACK_MASK), \
                              (uint32_t)((a) & PACK_MASK)) )
#define PCYCLE_GT(a,b)   ( !PCYCLE_LE((a),(b)) )
#define PCYCLE_GE(a,b)   ( !PCYCLE_LT((a),(b)) )
#define PCYCLE_DIFF(a,b) ( ((a)-(b)) & PACK_MASK )

#define PCYCLE_SANITY(stored, now, label)

#else
// === Pass-through 64-bit fallback (unchanged) ===
#undef PACK_MASK
#define PACK_MASK  (~0ULL)
#undef CYC_PACKED_MAX
#define CYC_PACKED_MAX UINT64_MAX
#define PACK_CYCLE(v) ((uint64_t)(v))
#define PCYCLE_LT(a,b)  ((uint64_t)(a) <  (uint64_t)(b))
#define PCYCLE_LE(a,b)  ((uint64_t)(a) <= (uint64_t)(b))
#define PCYCLE_GT(a,b)  ((uint64_t)(a) >  (uint64_t)(b))
#define PCYCLE_GE(a,b)  ((uint64_t)(a) >= (uint64_t)(b))
#define PCYCLE_DIFF(a,b) ((uint64_t)((a)-(b)))
#define PCYCLE_SANITY(stored, now, label)
#endif
