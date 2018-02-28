/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_RATELIMITER_H_
#define RTC_BASE_RATELIMITER_H_

#include "rtc_base/data_rate_limiter.h"
#include "rtc_base/deprecation.h"

namespace rtc {
// Deprecated, use DataRateLimiter instead
class RTC_DEPRECATED RateLimiter : public DataRateLimiter {
 public:
  RateLimiter(size_t max, double period) : DataRateLimiter(max, period) {}
};
}  // namespace rtc

#endif  // RTC_BASE_RATELIMITER_H_
