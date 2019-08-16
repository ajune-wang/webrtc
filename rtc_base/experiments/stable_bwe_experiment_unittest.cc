/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/stable_bwe_experiment.h"

#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {

TEST(StableBweExperimentTest, Default) {
  StableBweExperiment config = StableBweExperiment::ParseFromFieldTrials();
  EXPECT_FALSE(config.IsEnabled());
  EXPECT_FALSE(config.GetVideoHysteresisFactor());
  EXPECT_FALSE(config.GetScreenshareHysteresisFactor());
}

TEST(StableBweExperimentTest, EnabledNoHysteresis) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-StableBwe/enabled:true/");

  StableBweExperiment config = StableBweExperiment::ParseFromFieldTrials();
  EXPECT_TRUE(config.IsEnabled());
  EXPECT_FALSE(config.GetVideoHysteresisFactor());
  EXPECT_FALSE(config.GetScreenshareHysteresisFactor());
}

TEST(StableBweExperimentTest, EnabledWithHysteresis) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-StableBwe/"
      "enabled:true,"
      "video_hysteresis_factor:1.1,"
      "screenshare_hysteresis_factor:1.2/");

  StableBweExperiment config = StableBweExperiment::ParseFromFieldTrials();
  EXPECT_TRUE(config.IsEnabled());
  EXPECT_EQ(config.GetVideoHysteresisFactor(), 1.1);
  EXPECT_EQ(config.GetScreenshareHysteresisFactor(), 1.2);
}

TEST(StableBweExperimentTest, OnNoHysteresisPropagatesVideoRateHystersis) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-StableBwe/enabled:true/"
      "WebRTC-VideoRateControl/video_hysteresis:1.3,"
      "screenshare_hysteresis:1.4/");

  StableBweExperiment config = StableBweExperiment::ParseFromFieldTrials();
  EXPECT_TRUE(config.IsEnabled());
  EXPECT_EQ(config.GetVideoHysteresisFactor(), 1.3);
  EXPECT_EQ(config.GetScreenshareHysteresisFactor(), 1.4);
}

TEST(StableBweExperimentTest, HysteresisOverrideVideoRateHystersis) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-StableBwe/"
      "enabled:true,"
      "video_hysteresis_factor:1.1,"
      "screenshare_hysteresis_factor:1.2/"
      "WebRTC-VideoRateControl/video_hysteresis:1.3,"
      "screenshare_hysteresis:1.4/");

  StableBweExperiment config = StableBweExperiment::ParseFromFieldTrials();
  EXPECT_TRUE(config.IsEnabled());
  EXPECT_EQ(config.GetVideoHysteresisFactor(), 1.1);
  EXPECT_EQ(config.GetScreenshareHysteresisFactor(), 1.2);
}

}  // namespace webrtc
