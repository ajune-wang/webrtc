/*
 *
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/network_control/units/units_math.h"
#include "test/gtest.h"

namespace webrtc {
namespace units_impl {
TEST(UnitsMathTest, IntegerDivision) {
  EXPECT_EQ(DivideAndRound(1000, 1000), 1);
  EXPECT_EQ(DivideAndRound(501, 1000), 1);
  EXPECT_EQ(DivideAndRound(500, 1000), 1);
  EXPECT_EQ(DivideAndRound(499, 1000), 0);
  EXPECT_EQ(DivideAndRound(0, 1000), 0);
  EXPECT_EQ(DivideAndRound(-100, 1000), 0);
  EXPECT_EQ(DivideAndRound(-499, 1000), 0);
  EXPECT_EQ(DivideAndRound(-500, 1000), -1);
  EXPECT_EQ(DivideAndRound(-501, 1000), -1);
  EXPECT_EQ(DivideAndRound(-1000, 1000), -1);
}

TEST(UnitsMathTest, DoubleToInt) {
  EXPECT_EQ(DoubleToIntRounded(1.0), 1);
  EXPECT_EQ(DoubleToIntRounded(0.51), 1);
  EXPECT_EQ(DoubleToIntRounded(0.50), 1);
  EXPECT_EQ(DoubleToIntRounded(0.49), 0);
  EXPECT_EQ(DoubleToIntRounded(0.0), 0);
  EXPECT_EQ(DoubleToIntRounded(-0), 0);
  EXPECT_EQ(DoubleToIntRounded(-0.49), 0);
  EXPECT_EQ(DoubleToIntRounded(-0.50), -1);
  EXPECT_EQ(DoubleToIntRounded(-0.51), -1);
  EXPECT_EQ(DoubleToIntRounded(-1.0), -1);
}
}  // namespace units_impl
}  // namespace webrtc
