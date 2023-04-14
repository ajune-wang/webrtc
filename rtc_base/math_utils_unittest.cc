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

#include "test/gtest.h"

namespace rtc {

TEST(MathUtilsTest, GreatestCommonDivisor) {
  EXPECT_EQ(GreatestCommonDivisor(0, 1000), 1000);
  EXPECT_EQ(GreatestCommonDivisor(1, 1), 1);
  EXPECT_EQ(GreatestCommonDivisor(8, 12), 4);
  EXPECT_EQ(GreatestCommonDivisor(24, 54), 6);
}

TEST(MathUtilsTest, LeastCommonMultiple) {
  EXPECT_EQ(LeastCommonMultiple(1, 1), 1);
  EXPECT_EQ(LeastCommonMultiple(2, 3), 6);
  EXPECT_EQ(LeastCommonMultiple(16, 32), 32);
}

}  // namespace rtc
