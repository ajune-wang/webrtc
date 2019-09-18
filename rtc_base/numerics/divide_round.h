/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_DIVIDE_ROUND_H_
#define RTC_BASE_NUMERICS_DIVIDE_ROUND_H_

#include <type_traits>

#include "rtc_base/checks.h"

namespace webrtc {

template <typename Dividend, typename Divisor>
inline auto constexpr DivideRoundUp(Dividend dividend, Divisor divisor) {
  static_assert(std::is_integral<Dividend>::value, "");
  static_assert(std::is_integral<Divisor>::value, "");
  RTC_DCHECK_GE(dividend, 0);
  RTC_DCHECK_GT(divisor, 0);

  auto q = dividend / divisor;  // Quotient.
  auto r = dividend % divisor;  // Remainder.
  return q + (r > 0 ? 1 : 0);
}

template <typename Dividend, typename Divisor>
inline auto constexpr DivideRoundToNearest(Dividend dividend, Divisor divisor) {
  static_assert(std::is_integral<Dividend>::value, "");
  static_assert(std::is_integral<Divisor>::value, "");
  RTC_DCHECK_GE(dividend, 0);
  RTC_DCHECK_GT(divisor, 0);

  auto half_of_divisor = (divisor - 1) / 2;
  auto q = dividend / divisor;  // Quotient.
  auto r = dividend % divisor;  // Remainder.
  return q + (r > half_of_divisor ? 1 : 0);
}

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_DIVIDE_ROUND_H_
