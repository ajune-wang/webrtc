/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/bandwidth_scaler_settings.h"

#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(BandwidthScalerSettingsTest, ValuesNotSetByDefault) {
  const auto settings = BandwidthScalerSettings::ParseFromFieldTrials();
  EXPECT_FALSE(settings.BitrateStateUpdateInterval());
}

TEST(BandwidthScalerSettingsTest, ParseBitrateStateUpdateInterval) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BandwidthScalerSettings/"
      "bitrate_state_update_interval:100/");
  EXPECT_EQ(100u, BandwidthScalerSettings::ParseFromFieldTrials()
                      .BitrateStateUpdateInterval());
}

TEST(BandwidthScalerSettingsTest, ParseAll) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BandwidthScalerSettings/"
      "bitrate_state_update_interval:100/");
  EXPECT_EQ(100u, BandwidthScalerSettings::ParseFromFieldTrials()
                      .BitrateStateUpdateInterval());
}

TEST(BandwidthScalerSettingsTest, DoesNotParseIncorrectValue) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BandwidthScalerSettings/"
      "bitrate_state_update_interval:??/");
  const auto settings = BandwidthScalerSettings::ParseFromFieldTrials();
  EXPECT_FALSE(settings.BitrateStateUpdateInterval());
}

}  // namespace
}  // namespace webrtc
