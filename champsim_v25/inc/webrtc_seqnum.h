/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// ---------------------------------------------------------------------------
// VENDORED VERBATIM (BSD-3) from WebRTC, branch refs/heads/main:
//   rtc_base/numerics/sequence_number_util.h
//   rtc_base/numerics/mod_ops.h        (dependency, merged below)
// Source (raw): https://raw.githubusercontent.com/webrtc-mirror/webrtc/main/
//               rtc_base/numerics/sequence_number_util.h
//   (mirror of https://chromium.googlesource.com/external/webrtc/+/refs/heads/main)
// Vendored 2026-06-22 for champsim_v25 cycle_pack.h packed-cycle compares.
//
// DEVIATIONS FROM UPSTREAM (minimal, listed):
//   1. mod_ops.h was a separate WebRTC-internal include
//      ("rtc_base/numerics/mod_ops.h"); its required helpers (ForwardDiff,
//      ReverseDiff, MinDiff) are merged inline below so this header is
//      self-contained.
//   2. The WebRTC-internal include "rtc_base/checks.h" (unavailable) is
//      stripped. Its only use here is RTC_DCHECK_LT(a, M) debug assertions,
//      replaced by a no-op macro. This does not alter the AheadOf/AheadOrAt/
//      ForwardDiff/ReverseDiff/MinDiff *logic*, which is byte-for-byte
//      upstream.
//   3. SeqNumUnwrapper is NOT present in current upstream
//      sequence_number_util.h (moved to a separate WebRTC file in newer
//      revisions), so it is not vendored here. cycle_pack.h only needs
//      AheadOf / AheadOrAt, which are vendored verbatim.
// ---------------------------------------------------------------------------

#ifndef RTC_BASE_NUMERICS_SEQUENCE_NUMBER_UTIL_H_
#define RTC_BASE_NUMERICS_SEQUENCE_NUMBER_UTIL_H_

#include <algorithm>
#include <limits>
#include <type_traits>

// DEVIATION 2: WebRTC-internal "rtc_base/checks.h" stripped; RTC_DCHECK_LT
// is a debug-only bound assertion, replaced with a self-contained no-op.
#ifndef RTC_DCHECK_LT
#define RTC_DCHECK_LT(a, b) ((void)0)
#endif

namespace webrtc {

// ===== from rtc_base/numerics/mod_ops.h (merged, DEVIATION 1) =============

// Calculates the forward difference between two wrapping numbers.
//
// If M > 0 then wrapping occurs at M, if M == 0 then wrapping occurs at the
// largest value representable by T.
template <typename T, T M>
inline typename std::enable_if<(M > 0), T>::type ForwardDiff(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  RTC_DCHECK_LT(a, M);
  RTC_DCHECK_LT(b, M);
  return a <= b ? b - a : M - (a - b);
}

template <typename T, T M>
inline typename std::enable_if<(M == 0), T>::type ForwardDiff(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  return b - a;
}

template <typename T>
inline T ForwardDiff(T a, T b) {
  return ForwardDiff<T, 0>(a, b);
}

// Calculates the reverse difference between two wrapping numbers.
//
// If M > 0 then wrapping occurs at M, if M == 0 then wrapping occurs at the
// largest value representable by T.
template <typename T, T M>
inline typename std::enable_if<(M > 0), T>::type ReverseDiff(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  RTC_DCHECK_LT(a, M);
  RTC_DCHECK_LT(b, M);
  return b <= a ? a - b : M - (b - a);
}

template <typename T, T M>
inline typename std::enable_if<(M == 0), T>::type ReverseDiff(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  return a - b;
}

template <typename T>
inline T ReverseDiff(T a, T b) {
  return ReverseDiff<T, 0>(a, b);
}

// Calculates the minimum distance between to wrapping numbers.
//
// The minimum distance is defined as min(ForwardDiff(a, b), ReverseDiff(a, b))
template <typename T, T M = 0>
inline T MinDiff(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  return std::min(ForwardDiff<T, M>(a, b), ReverseDiff<T, M>(a, b));
}

// ===== from rtc_base/numerics/sequence_number_util.h (verbatim) ===========

// Test if the sequence number `a` is ahead or at sequence number `b`.
//
// If `M` is an even number and the two sequence numbers are at max distance
// from each other, then the sequence number with the highest value is
// considered to be ahead.
template <typename T, T M>
inline typename std::enable_if<(M > 0), bool>::type AheadOrAt(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  const T maxDist = M / 2;
  if (!(M & 1) && MinDiff<T, M>(a, b) == maxDist)
    return b < a;
  return ForwardDiff<T, M>(b, a) <= maxDist;
}

template <typename T, T M>
inline typename std::enable_if<(M == 0), bool>::type AheadOrAt(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  const T maxDist = std::numeric_limits<T>::max() / 2 + T(1);
  if (a - b == maxDist)
    return b < a;
  return ForwardDiff(b, a) < maxDist;
}

template <typename T>
inline bool AheadOrAt(T a, T b) {
  return AheadOrAt<T, 0>(a, b);
}

// Test if the sequence number `a` is ahead of sequence number `b`.
//
// If `M` is an even number and the two sequence numbers are at max distance
// from each other, then the sequence number with the highest value is
// considered to be ahead.
template <typename T, T M = 0>
inline bool AheadOf(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  return a != b && AheadOrAt<T, M>(a, b);
}

// Comparator used to compare sequence numbers in a continuous fashion.
//
// WARNING! If used to sort sequence numbers of length M then the interval
//          covered by the sequence numbers may not be larger than floor(M/2).
template <typename T, T M = 0>
struct AscendingSeqNumComp {
  bool operator()(T a, T b) const { return AheadOf<T, M>(a, b); }
};

// Comparator used to compare sequence numbers in a continuous fashion.
//
// WARNING! If used to sort sequence numbers of length M then the interval
//          covered by the sequence numbers may not be larger than floor(M/2).
template <typename T, T M = 0>
struct DescendingSeqNumComp {
  bool operator()(T a, T b) const { return AheadOf<T, M>(b, a); }
};

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_SEQUENCE_NUMBER_UTIL_H_
