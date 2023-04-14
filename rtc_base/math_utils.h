/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MATH_UTILS_H_
#define RTC_BASE_MATH_UTILS_H_

namespace rtc {

// Returns the largest positive integer that divides both `a` and `b`.
int GreatestCommonDivisor(int a, int b);

// Returns the smallest positive integer that is divisible by both `a` and `b`.
int LeastCommonMultiple(int a, int b);

}  // namespace rtc

#endif  // RTC_BASE_MATH_UTILS_H_
