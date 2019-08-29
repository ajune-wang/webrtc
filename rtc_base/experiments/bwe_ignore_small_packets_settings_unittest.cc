/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/bwe_ignore_small_packets_settings.h"

#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(BweIgnoreSmallPacketsSettingsTest, ParsesIgnoredSize) {
  EXPECT_EQ(
      BweIgnoreSmallPacketsSettings::ParseFromFieldTrials().ignored_size(), 0u);

  test::ScopedFieldTrials field_trials(
      "WebRTC-BweIgnoreSmallPackets/ignored_size:210/");
  EXPECT_EQ(
      BweIgnoreSmallPacketsSettings::ParseFromFieldTrials().ignored_size(),
      210u);
}

TEST(BweIgnoreSmallPacketsSettingsTest, ParsesMinFractionLargePackets) {
  EXPECT_EQ(BweIgnoreSmallPacketsSettings::ParseFromFieldTrials()
                .min_fraction_large_packets(),
            1.0);

  test::ScopedFieldTrials field_trials(
      "WebRTC-BweIgnoreSmallPackets/min_fraction_large_packets:0.25/");
  EXPECT_EQ(BweIgnoreSmallPacketsSettings::ParseFromFieldTrials()
                .min_fraction_large_packets(),
            0.25);
}

TEST(BweIgnoreSmallPacketsSettingsTest, ParsesLargePacketSize) {
  EXPECT_EQ(
      BweIgnoreSmallPacketsSettings::ParseFromFieldTrials().large_packet_size(),
      0u);

  test::ScopedFieldTrials field_trials(
      "WebRTC-BweIgnoreSmallPackets/ignored_size:345/");
  EXPECT_EQ(
      BweIgnoreSmallPacketsSettings::ParseFromFieldTrials().large_packet_size(),
      345u);
}

}  // namespace
}  // namespace webrtc
