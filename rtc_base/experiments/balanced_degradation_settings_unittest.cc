/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/balanced_degradation_settings.h"

#include <limits>

#include "rtc_base/gunit.h"
#include "test/field_trial.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

void VerifyIsDefault(
    const std::vector<BalancedDegradationSettings::Config>& config) {
  EXPECT_THAT(config, ::testing::ElementsAre(
                          BalancedDegradationSettings::Config{
                              320 * 240, 7, {0, 0}, {0, 0}, {0, 0}},
                          BalancedDegradationSettings::Config{
                              480 * 270, 10, {0, 0}, {0, 0}, {0, 0}},
                          BalancedDegradationSettings::Config{
                              640 * 480, 15, {0, 0}, {0, 0}, {0, 0}}));
}
}  // namespace

TEST(BalancedDegradationSettings, GetsDefaultConfigIfNoList) {
  webrtc::test::ScopedFieldTrials field_trials("");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
  EXPECT_FALSE(settings.QpLowThreshold(kVideoCodecVP8, 1));
  EXPECT_FALSE(settings.QpHighThreshold(kVideoCodecVP8, 1));
  EXPECT_FALSE(settings.QpLowThreshold(kVideoCodecH264, 1));
  EXPECT_FALSE(settings.QpHighThreshold(kVideoCodecH264, 1));
  EXPECT_FALSE(settings.QpLowThreshold(kVideoCodecGeneric, 1));
  EXPECT_FALSE(settings.QpHighThreshold(kVideoCodecGeneric, 1));
  EXPECT_FALSE(settings.QpLowThreshold(kVideoCodecVP9, 1));
  EXPECT_FALSE(settings.QpHighThreshold(kVideoCodecVP9, 1));
}

TEST(BalancedDegradationSettings, GetsConfig) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:11|22|33,fps:5|15|25,other:4|5|6/");
  BalancedDegradationSettings settings;
  EXPECT_THAT(
      settings.GetConfigs(),
      ::testing::ElementsAre(
          BalancedDegradationSettings::Config{11, 5, {0, 0}, {0, 0}, {0, 0}},
          BalancedDegradationSettings::Config{22, 15, {0, 0}, {0, 0}, {0, 0}},
          BalancedDegradationSettings::Config{33, 25, {0, 0}, {0, 0}, {0, 0}}));
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForZeroFpsValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:0|15|25/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfPixelsDecreases) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|999|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfFramerateDecreases) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|4|25/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsMinFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(5, settings.MinFps(1));
  EXPECT_EQ(5, settings.MinFps(1000));
  EXPECT_EQ(15, settings.MinFps(1000 + 1));
  EXPECT_EQ(15, settings.MinFps(2000));
  EXPECT_EQ(25, settings.MinFps(2000 + 1));
  EXPECT_EQ(25, settings.MinFps(3000));
  EXPECT_EQ(std::numeric_limits<int>::max(), settings.MinFps(3000 + 1));
}

TEST(BalancedDegradationSettings, GetsMaxFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(15, settings.MaxFps(1));
  EXPECT_EQ(15, settings.MaxFps(1000));
  EXPECT_EQ(25, settings.MaxFps(1000 + 1));
  EXPECT_EQ(25, settings.MaxFps(2000));
  EXPECT_EQ(std::numeric_limits<int>::max(), settings.MaxFps(2000 + 1));
}

TEST(BalancedDegradationSettings, QpThresholdsNotSetByDefault) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  EXPECT_FALSE(settings.QpLowThreshold(kVideoCodecVP8, 1));
  EXPECT_FALSE(settings.QpHighThreshold(kVideoCodecVP8, 1));
  EXPECT_FALSE(settings.QpLowThreshold(kVideoCodecH264, 1));
  EXPECT_FALSE(settings.QpHighThreshold(kVideoCodecH264, 1));
  EXPECT_FALSE(settings.QpLowThreshold(kVideoCodecGeneric, 1));
  EXPECT_FALSE(settings.QpHighThreshold(kVideoCodecGeneric, 1));
}

TEST(BalancedDegradationSettings, GetsConfigWithQpThresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_qp_low:22|21|20,"
      "vp8_qp_high:90|95|100,h264_qp_low:12|13|14,h264_qp_high:20|30|40,"
      "generic_qp_low:7|6|5,generic_qp_high:22|23|24/");
  BalancedDegradationSettings settings;
  EXPECT_THAT(settings.GetConfigs(),
              ::testing::ElementsAre(
                  BalancedDegradationSettings::Config{
                      1000, 5, {22, 90}, {12, 20}, {7, 22}},
                  BalancedDegradationSettings::Config{
                      2000, 15, {21, 95}, {13, 30}, {6, 23}},
                  BalancedDegradationSettings::Config{
                      3000, 25, {20, 100}, {14, 40}, {5, 24}}));
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForVp8ZeroQpHighValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_qp_high:0|95|96/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForVp8ZeroQpLowValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_qp_low:25|0|26/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForH264ZeroQpHighValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,h264_qp_high:37|0|38/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForH264ZeroQpLowValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,h264_qp_low:0|12|13/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForGenericZeroQpHighValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,generic_qp_high:22|23|0/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForGenericZeroQpLowValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,generic_qp_low:12|0|13/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsVp8HighQpThreshold) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_qp_high:85|80|90/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(85, settings.QpHighThreshold(kVideoCodecVP8, 1));
  EXPECT_EQ(85, settings.QpHighThreshold(kVideoCodecVP8, 1000));
  EXPECT_EQ(80, settings.QpHighThreshold(kVideoCodecVP8, 1001));
  EXPECT_EQ(80, settings.QpHighThreshold(kVideoCodecVP8, 2000));
  EXPECT_EQ(90, settings.QpHighThreshold(kVideoCodecVP8, 2001));
  EXPECT_EQ(90, settings.QpHighThreshold(kVideoCodecVP8, 3000));
  EXPECT_EQ(90, settings.QpHighThreshold(kVideoCodecVP8, 3001));
}

TEST(BalancedDegradationSettings, GetsVp8LowQpThreshold) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_qp_low:30|31|32/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(30, settings.QpLowThreshold(kVideoCodecVP8, 1000));
}

TEST(BalancedDegradationSettings, GetsH264HighQpThreshold) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,h264_qp_high:41|43|42/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(43, settings.QpHighThreshold(kVideoCodecH264, 2000));
}

TEST(BalancedDegradationSettings, GetsH264LowQpThreshold) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,h264_qp_low:21|22|23/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(22, settings.QpLowThreshold(kVideoCodecH264, 2000));
}

TEST(BalancedDegradationSettings, GetsGenericHighQpThreshold) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,generic_qp_high:22|23|24/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(24, settings.QpHighThreshold(kVideoCodecGeneric, 3000));
}

TEST(BalancedDegradationSettings, GetsGenericLowQpThreshold) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,generic_qp_low:2|3|4/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(4, settings.QpLowThreshold(kVideoCodecGeneric, 3000));
}

}  // namespace webrtc
