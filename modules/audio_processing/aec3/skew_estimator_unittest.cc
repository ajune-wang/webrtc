/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/skew_estimator.h"

#include "test/gtest.h"

namespace webrtc {
namespace aec3 {

TEST(SkewEstimator, SkewChange) {
  constexpr int kNumSkewsLog2 = 7;
  constexpr int kNumSkews = 1 << kNumSkewsLog2;

  SkewEstimator estimator(kNumSkewsLog2);

  for (int k = 0; k < kNumSkews - 1; ++k) {
    estimator.LogRenderCall();
    auto skew = estimator.GetSkewFromCapture();
    EXPECT_FALSE(skew);
  }

  estimator.LogRenderCall();

  rtc::Optional<int> skew;
  for (int k = 0; k < kNumSkews; ++k) {
    estimator.LogRenderCall();
    skew = estimator.GetSkewFromCapture();
    EXPECT_TRUE(skew);
  }
  EXPECT_EQ(1, *skew);

  estimator.LogRenderCall();

  for (int k = 0; k < kNumSkews; ++k) {
    estimator.LogRenderCall();
    skew = estimator.GetSkewFromCapture();
    EXPECT_TRUE(skew);
  }
  EXPECT_EQ(2, *skew);
}

TEST(SkewEstimator, Skew1) {
  constexpr int kNumSkewsLog2 = 7;
  constexpr int kNumSkews = 1 << kNumSkewsLog2;

  SkewEstimator estimator(kNumSkewsLog2);

  for (int k = 0; k < kNumSkews - 1; ++k) {
    estimator.LogRenderCall();
    auto skew = estimator.GetSkewFromCapture();
    EXPECT_FALSE(skew);
  }

  estimator.LogRenderCall();

  rtc::Optional<int> skew;
  for (int k = 0; k < kNumSkews; ++k) {
    estimator.LogRenderCall();
    skew = estimator.GetSkewFromCapture();
    EXPECT_TRUE(skew);
  }
  EXPECT_EQ(1, *skew);
}

TEST(SkewEstimator, SkewMinus) {
  constexpr int kNumSkewsLog2 = 7;
  constexpr int kNumSkews = 1 << kNumSkewsLog2;

  SkewEstimator estimator(kNumSkewsLog2);

  for (int k = 0; k < kNumSkews - 1; ++k) {
    estimator.LogRenderCall();
    auto skew = estimator.GetSkewFromCapture();
    EXPECT_FALSE(skew);
  }

  rtc::Optional<int> skew;
  skew = estimator.GetSkewFromCapture();

  for (int k = 0; k < kNumSkews; ++k) {
    estimator.LogRenderCall();
    skew = estimator.GetSkewFromCapture();
    EXPECT_TRUE(skew);
  }
  EXPECT_EQ(-1, *skew);
}

TEST(SkewEstimator, NullEstimate) {
  constexpr int kNumSkewsLog2 = 4;
  constexpr int kNumSkews = 1 << kNumSkewsLog2;

  SkewEstimator estimator(kNumSkewsLog2);

  for (int k = 0; k < kNumSkews - 1; ++k) {
    estimator.LogRenderCall();
    auto skew = estimator.GetSkewFromCapture();
    EXPECT_FALSE(skew);
  }

  estimator.LogRenderCall();
  auto skew = estimator.GetSkewFromCapture();
  EXPECT_TRUE(skew);

  estimator.Reset();
  for (int k = 0; k < kNumSkews - 1; ++k) {
    estimator.LogRenderCall();
    auto skew = estimator.GetSkewFromCapture();
    EXPECT_FALSE(skew);
  }
}
}  // namespace aec3
}  // namespace webrtc
