/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/quality_scaler_settings.h"

#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(QualityScalerSettingsTest, ValuesNotSetByDefault) {
  EXPECT_FALSE(QualityScalerSettings::ParseFromFieldTrials().MinFrames());
  EXPECT_FALSE(QualityScalerSettings::ParseFromFieldTrials().ScaleFactor());
  EXPECT_FALSE(QualityScalerSettings::ParseFromFieldTrials().FastScaleFactor());
}

TEST(QualityScalerSettingsTest, ParseMinFrames) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/min_frames:100/");
  EXPECT_EQ(100, QualityScalerSettings::ParseFromFieldTrials().MinFrames());
}

TEST(QualityScalerSettingsTest, ParseScaleFactor) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/scale_factor:1.5/");
  EXPECT_EQ(1.5, QualityScalerSettings::ParseFromFieldTrials().ScaleFactor());
}

TEST(QualityScalerSettingsTest, ParseFastScaleFactor) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/fast_scale_factor:1.1/");
  EXPECT_EQ(1.1,
            QualityScalerSettings::ParseFromFieldTrials().FastScaleFactor());
}

TEST(QualityScalerSettingsTest, ParseAll) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/"
      "min_frames:100,scale_factor:1.5,fast_scale_factor:0.9/");
  const auto settings = QualityScalerSettings::ParseFromFieldTrials();
  EXPECT_EQ(100, settings.MinFrames());
  EXPECT_EQ(1.5, settings.ScaleFactor());
  EXPECT_EQ(0.9, settings.FastScaleFactor());
}

TEST(QualityScalerSettingsTest, DoesNotParseIncorrectValue) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/"
      "min_frames:a,scale_factor:b,fast_scale_factor:c/");
  const auto settings = QualityScalerSettings::ParseFromFieldTrials();
  EXPECT_FALSE(settings.MinFrames());
  EXPECT_FALSE(settings.ScaleFactor());
  EXPECT_FALSE(settings.FastScaleFactor());
}

TEST(QualityScalerSettingsTest, DoesNotReturnTooSmallValue) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/"
      "min_frames:0,scale_factor:0.0,fast_scale_factor:0.0/");
  const auto settings = QualityScalerSettings::ParseFromFieldTrials();
  EXPECT_FALSE(settings.MinFrames());
  EXPECT_FALSE(settings.ScaleFactor());
  EXPECT_FALSE(settings.FastScaleFactor());
}

}  // namespace
}  // namespace webrtc
