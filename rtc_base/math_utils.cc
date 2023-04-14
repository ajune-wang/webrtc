/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/math_utils.h"

#include "rtc_base/checks.h"

namespace rtc {

int GreatestCommonDivisor(int a, int b) {
  RTC_DCHECK_GE(a, 0);
  RTC_DCHECK_GT(b, 0);
  int c = a % b;
  while (c != 0) {
    a = b;
    b = c;
    c = a % b;
  }
  return b;
}

int LeastCommonMultiple(int a, int b) {
  RTC_DCHECK_GT(a, 0);
  RTC_DCHECK_GT(b, 0);
  return a * (b / GreatestCommonDivisor(a, b));
}

}  // namespace rtc
