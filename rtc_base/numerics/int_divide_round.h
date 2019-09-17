/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_INT_DIVIDE_ROUND_H_
#define RTC_BASE_NUMERICS_INT_DIVIDE_ROUND_H_

#include <type_traits>

#include "rtc_base/checks.h"

namespace webrtc {

template <typename Dividend, typename Divisor>
inline auto constexpr IntDivideRoundUp(Dividend dividend, Divisor divisor) {
  static_assert(std::is_integral<Dividend>::value, "");
  static_assert(std::is_integral<Divisor>::value, "");
  RTC_DCHECK_GE(dividend, 0);
  RTC_DCHECK_GT(divisor, 0);

  auto result = dividend / divisor;
  if (dividend % divisor > 0)
    ++result;
  return result;
}

template <typename Dividend, typename Divisor>
inline auto constexpr IntDivideRoundToNearest(Dividend dividend,
                                              Divisor divisor) {
  static_assert(std::is_integral<Dividend>::value, "");
  static_assert(std::is_integral<Divisor>::value, "");
  RTC_DCHECK_GE(dividend, 0);
  RTC_DCHECK_GT(divisor, 0);

  // Same as IntDivideRoundUp(divisor, 2).
  Divisor half = (divisor / 2) + (divisor % 2);

  auto result = dividend / divisor;
  if (dividend % divisor >= half)
    ++result;
  return result;
}

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_INT_DIVIDE_ROUND_H_
