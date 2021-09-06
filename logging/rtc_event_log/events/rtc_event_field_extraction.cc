/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_field_extraction.h"

#include <algorithm>
#include <limits>

#include "rtc_base/checks.h"

namespace webrtc_event_logging {

// TODO(terelius): Decide whether to use the naive linear-complexity
// code to compute bitwidth/base-2 logarithms, or the logarithmic
// version below.
// The logarithmic version is marginally slower for small numbers,
// but several times faster for large numbers. We'll need to look
// at data from actual calls to know the distribution of real inputs.
// Until then, we optimize for the worst case.
// uint64_t UnsignedBitWidth(uint64_t input, bool zero_val_as_zero_width) {
//  if (zero_val_as_zero_width && input==0) {
//    return 0;
//  }
//  uint64_t log = 0;
//  while (input>>=1) {
//    ++log;
//  }
//  return log + 1;
//}

uint64_t UnsignedBitWidth(uint64_t x, bool zero_val_as_zero_width) {
  uint64_t log = 0;
  // Check if anything is set in the top 32 bits of the 64 bit input,
  // then check top 16 bits of the remaining 32 bit word,
  // then top 8 bits of the remaining 16 bit half-word and so on.
  if (x & 0xFFFFFFFF00000000ull) {
    x >>= 32;
    log |= 32;
  }
  if (x & 0xFFFF0000ull) {
    x >>= 16;
    log |= 16;
  }
  if (x & 0xFF00ull) {
    x >>= 8;
    log |= 8;
  }
  if (x & 0xF0ull) {
    x >>= 4;
    log |= 4;
  }
  if (x & 0xCull) {
    x >>= 2;
    log |= 2;
  }
  if (x & 0x2ull) {
    x >>= 1;
    log |= 1;
  }
  if (zero_val_as_zero_width) {
    // `x` is now either 0 (if the original input was 0) or 1 (otherwise).
    return log + x;
  }
  return log + 1;
}

uint64_t SignedBitWidth(uint64_t max_pos_magnitude,
                        uint64_t max_neg_magnitude) {
  const uint64_t bitwidth_pos = UnsignedBitWidth(max_pos_magnitude, true);
  const uint64_t bitwidth_neg =
      (max_neg_magnitude > 0) ? UnsignedBitWidth(max_neg_magnitude - 1, true)
                              : 0;
  return 1 + std::max(bitwidth_pos, bitwidth_neg);
}

// Return the maximum integer of a given bit width.
uint64_t MaxUnsignedValueOfBitWidth(uint64_t bit_width) {
  RTC_DCHECK_GE(bit_width, 1);
  RTC_DCHECK_LE(bit_width, 64);
  return (bit_width == 64) ? std::numeric_limits<uint64_t>::max()
                           : ((static_cast<uint64_t>(1) << bit_width) - 1);
}

// Computes the delta between |previous| and |current|, under the assumption
// that `bit_mask` is the largest value before wrap-around occurs. The bitmask
// must be of the form 2^x-1. (We use the wrap-around to more efficiently
// compress counters that wrap around at different bit widths than the
// backing C++ data type.)
uint64_t UnsignedDelta(uint64_t previous, uint64_t current, uint64_t bit_mask) {
  RTC_DCHECK_LE(previous, bit_mask);
  RTC_DCHECK_LE(current, bit_mask);
  return (current - previous) & bit_mask;
}

}  // namespace webrtc_event_logging
