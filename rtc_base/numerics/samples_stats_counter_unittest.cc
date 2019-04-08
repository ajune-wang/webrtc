/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/samples_stats_counter.h"

#include <math.h>
#include <random>
#include <vector>

#include "absl/algorithm/container.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

SamplesStatsCounter CreateStatsFilledWithIntsFrom1ToN(int n) {
  std::vector<double> data;
  for (int i = 1; i <= n; i++) {
    data.push_back(i);
  }
  absl::c_shuffle(data, std::mt19937(std::random_device()()));

  SamplesStatsCounter stats;
  for (double v : data) {
    stats.AddSample(v);
  }
  return stats;
}

// Add n samples drawn from uniform distribution in [a;b].
SamplesStatsCounter CreateStatsFromUniformDistribution(int n,
                                                       double a,
                                                       double b) {
  std::mt19937 gen{std::random_device()()};
  std::uniform_real_distribution<> dis(a, b);

  SamplesStatsCounter stats;
  for (int i = 1; i <= n; i++) {
    stats.AddSample(dis(gen));
  }
  return stats;
}

}  // namespace

TEST(SamplesStatsCounter, FullSimpleTest) {
  SamplesStatsCounter stats = CreateStatsFilledWithIntsFrom1ToN(100);

  EXPECT_TRUE(!stats.IsEmpty());
  EXPECT_DOUBLE_EQ(stats.GetMin(), 1.0);
  EXPECT_DOUBLE_EQ(stats.GetMax(), 100.0);
  EXPECT_DOUBLE_EQ(stats.GetAverage(), 50.5);
  for (int i = 1; i <= 100; i++) {
    double p = i / 100.0;
    EXPECT_GE(stats.GetPercentile(p), i);
    EXPECT_LT(stats.GetPercentile(p), i + 1);
  }
}

TEST(SamplesStatsCounter, VarianceAndDeviation) {
  SamplesStatsCounter stats;
  stats.AddSample(2);
  stats.AddSample(2);
  stats.AddSample(-1);
  stats.AddSample(5);

  EXPECT_DOUBLE_EQ(stats.GetAverage(), 2.0);
  EXPECT_DOUBLE_EQ(stats.GetVariance(), 4.5);
  EXPECT_DOUBLE_EQ(stats.GetStandardDeviation(), sqrt(4.5));
}

TEST(SamplesStatsCounter, FractionPercentile) {
  SamplesStatsCounter stats = CreateStatsFilledWithIntsFrom1ToN(5);

  EXPECT_DOUBLE_EQ(stats.GetPercentile(0.5), 3);
}

TEST(SamplesStatsCounter, TestBorderValues) {
  SamplesStatsCounter stats = CreateStatsFilledWithIntsFrom1ToN(5);

  EXPECT_GE(stats.GetPercentile(0.01), 1);
  EXPECT_LT(stats.GetPercentile(0.01), 2);
  EXPECT_DOUBLE_EQ(stats.GetPercentile(1.0), 5);
}

TEST(SamplesStatsCounter, VarianceFromUniformDistribution) {
  // Check variance converge to 1/12 for [0;1) uniform distribution.
  // Acts as a sanity check for NumericStabilityForVariance test.
  SamplesStatsCounter stats = CreateStatsFromUniformDistribution(1e6, 0, 1);

  EXPECT_NEAR(stats.GetVariance(), 1. / 12, 1e-3);
}

TEST(SamplesStatsCounter, NumericStabilityForVariance) {
  // Same test than VarianceFromUniformDistribution,
  // except the range is shifted to [1e9;1e9+1).
  // Variance should also converge to 1/12.
  // NB: Although we lose precision for the samples themselves, the fractional
  //     part still enjoys 22 bits of mantissa and errors should even out,
  //     so that couldn't explain a mismatch.
  SamplesStatsCounter stats =
      CreateStatsFromUniformDistribution(1e6, 1e9, 1e9 + 1);

  EXPECT_NEAR(stats.GetVariance(), 1. / 12, 1e-3);
}

}  // namespace webrtc
