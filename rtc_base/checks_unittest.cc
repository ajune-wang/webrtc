/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/checks.h"

#include "test/gtest.h"

TEST(ChecksTest, ExpressionNotEvaluatedWhenCheckPassing) {
  int i = 0;
  RTC_CHECK(true) << "i=" << ++i;
  RTC_CHECK_EQ(i, 0) << "Previous check passed, but i was incremented!";
}
