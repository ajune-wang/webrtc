/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/units/quantities.h"

#include "test/gtest.h"

namespace webrtc {
namespace test {

TEST(Quantity, Add) {
  TimeDelta d = TimeDelta::Seconds(1);
  EXPECT_EQ(d + d, TimeDelta::Seconds(2));
  EXPECT_EQ((d + d).seconds(), 2.0);
}

TEST(Quantity, Sub) {
  TimeDelta d = TimeDelta::Seconds(1);
  EXPECT_EQ(d - d, TimeDelta::Seconds(0));
  EXPECT_EQ((d - d).seconds(), 0.0);
}

TEST(Quantity, Scalar) {
  DataSize s1 = DataSize::Bits(1000);
  EXPECT_EQ(s1 * 2.5, DataSize::Bits(2500));
  EXPECT_EQ((s1 * 2.5).Bits(), 2500.0);
}

TEST(Quantity, Ratio) {
  DataSize s1 = DataSize::Bits(1000);
  DataSize s2 = DataSize::Bits(200);

  EXPECT_EQ(s1 / s2, 5.0);
}

TEST(Quantity, Mult) {
  DataSize data_size = DataSize::Bits(100);
  DataRate data_rate = DataRate::BitsPerSec(500);
  TimeDelta time_delta = TimeDelta::Seconds(10);
  Frequency frequency = Frequency::Hz(10);

  EXPECT_EQ((data_size * frequency).BitsPerSec(), 1000.0);
  EXPECT_EQ((frequency * data_size).BitsPerSec(), 1000.0);

  EXPECT_EQ((data_rate * time_delta).Bits(), 5000.0);
  EXPECT_EQ((time_delta * data_rate).Bits(), 5000.0);

  EXPECT_EQ(time_delta * frequency, 100.0);
  EXPECT_EQ(frequency * time_delta, 100.0);
}

TEST(Quantity, Div) {
  DataSize data_size = DataSize::Bits(100);
  DataRate data_rate = DataRate::BitsPerSec(500);
  TimeDelta time_delta = TimeDelta::Seconds(10);
  Frequency frequency = Frequency::Hz(10);

  EXPECT_EQ((data_size / data_rate).seconds(), 0.2);
  EXPECT_EQ((data_rate / data_size).hz(), 5.0);

  EXPECT_EQ((data_size / time_delta).BitsPerSec(), 10.0);

  EXPECT_EQ((data_rate / frequency).Bits(), 50);
}

#if !ONLY_ALLOW_DEFINED_QUANTITIES
TEST(Quantity, WhatIsThisQuantity) {
  auto data_rate = DataRate::BitsPerSec(10);

  // What does a Quantity<-3, 3> represent? No idea, but we can have one!
  // Maybe useful for intermediate quantities when calculating?
  auto wtf = data_rate * data_rate * data_rate;
  static_assert(std::is_same_v<Quantity<-3, 3>, decltype(wtf)>, "");
}
#endif

}  // namespace test
}  // namespace webrtc
