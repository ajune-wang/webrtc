/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/delays/delay_variation_calculator.h"

#include "api/numerics/samples_stats_counter.h"
#include "api/units/timestamp.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

MATCHER_P(HasLength, s, "") {
  bool c1 = arg.rtp_timestamps.NumSamples() == s;
  bool c2 = arg.arrival_times_ms.NumSamples() == s;
  bool c3 = arg.sizes_bytes.NumSamples() == s;
  bool c4 = arg.inter_departure_times_ms.NumSamples() == s;
  bool c5 = arg.inter_arrival_times_ms.NumSamples() == s;
  bool c6 = arg.inter_delay_variations_ms.NumSamples() == s;
  bool c7 = arg.inter_size_variations_bytes.NumSamples() == s;
  *result_listener << "\nc: " << c1 << c2 << c3 << c4 << c5 << c6 << c7;
  return c1 && c2 && c3 && c4 && c5 && c6 && c7;
}

TEST(DelayVariationCalculatorTest, NoTimeSeriesWithoutFrame) {
  DelayVariationCalculator calc;

  EXPECT_THAT(calc.time_series(), HasLength(0));
}

TEST(DelayVariationCalculatorTest, PartialTimeSeriesWithOneFrame) {
  DelayVariationCalculator calc;

  calc.Insert(3000, Timestamp::Millis(33), DataSize::Bytes(100));

  const DelayVariationCalculator::TimeSeries ts = calc.time_series();
  ASSERT_THAT(calc.time_series(), HasLength(1));
  auto v0 = [](const SamplesStatsCounter& c) {
    return c.GetTimedSamples()[0].value;
  };
  EXPECT_EQ(v0(ts.rtp_timestamps), 3000);
  EXPECT_EQ(v0(ts.arrival_times_ms), 33);
  EXPECT_EQ(v0(ts.sizes_bytes), 100);
  EXPECT_EQ(v0(ts.inter_departure_times_ms), 0);
  EXPECT_EQ(v0(ts.inter_arrival_times_ms), 0);
  EXPECT_EQ(v0(ts.inter_delay_variations_ms), 0);
  EXPECT_EQ(v0(ts.inter_size_variations_bytes), 0);
}

TEST(DelayVariationCalculatorTest, TimeSeriesWithTwoFrames) {
  DelayVariationCalculator calc;

  calc.Insert(3000, Timestamp::Millis(33), DataSize::Bytes(100));
  calc.Insert(6000, Timestamp::Millis(66), DataSize::Bytes(100));

  const DelayVariationCalculator::TimeSeries ts = calc.time_series();
  ASSERT_THAT(calc.time_series(), HasLength(2));
  auto v1 = [](const SamplesStatsCounter& c) {
    return c.GetTimedSamples()[1].value;
  };
  EXPECT_EQ(v1(ts.rtp_timestamps), 6000);
  EXPECT_EQ(v1(ts.arrival_times_ms), 66);
  EXPECT_EQ(v1(ts.sizes_bytes), 100);
  EXPECT_EQ(v1(ts.inter_departure_times_ms), 33.333);
  EXPECT_EQ(v1(ts.inter_arrival_times_ms), 33);
  EXPECT_EQ(v1(ts.inter_delay_variations_ms), -0.333);
  EXPECT_EQ(v1(ts.inter_size_variations_bytes), 0);
}

TEST(DelayVariationCalculatorTest, Metadata) {}

}  // namespace test
}  // namespace webrtc
