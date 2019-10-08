/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/performance_stats.h"

#include "test/gtest.h"

namespace webrtc {
namespace test {

TEST(PerformanceStatsTest, DurationZeroWhenNoFrames) {
  EventRateCounter event_rate_counter;
  EXPECT_EQ(event_rate_counter.Duration(), TimeDelta::Zero());
}

TEST(PerformanceStatsTest, DurationZeroWithOneFrame) {
  EventRateCounter event_rate_counter;
  event_rate_counter.AddEvent(Timestamp::ms(1000));
  EXPECT_EQ(event_rate_counter.Duration(), TimeDelta::Zero());
}

TEST(PerformanceStatsTest, DurationWithMultipleFrames) {
  EventRateCounter event_rate_counter;
  event_rate_counter.AddEvent(Timestamp::seconds(1));
  event_rate_counter.AddEvent(Timestamp::seconds(2));
  EXPECT_EQ(event_rate_counter.Duration(), TimeDelta::seconds(1));
}

}  // namespace test
}  // namespace webrtc
