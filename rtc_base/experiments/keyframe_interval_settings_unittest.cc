/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/keyframe_interval_settings.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(KeyframeIntervalSettingsTest, MinKeyframeSendIntervalMsSet) {
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MinKeyframeSendIntervalMs());

  test::ScopedFieldTrials field_trials(
      "WebRTC-KeyframeInterval/min_keyframe_send_interval_ms:100/");
  EXPECT_EQ(KeyframeIntervalSettings::ParseFromFieldTrials()
                .MinKeyframeSendIntervalMs(),
            100);
}

TEST(KeyframeIntervalSettingsTest, MaxWaitForReceivedKeyframeMsSet) {
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MaxWaitForReceivedKeyframeMs());

  test::ScopedFieldTrials field_trials(
      "WebRTC-KeyframeInterval/max_wait_for_received_keyframe_ms:100/");
  EXPECT_EQ(KeyframeIntervalSettings::ParseFromFieldTrials()
                .MaxWaitForReceivedKeyframeMs(),
            100);
}

TEST(KeyframeIntervalSettingsTest, MaxWaitForReceivedFrameMsSet) {
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MaxWaitForReceivedFrameMs());

  test::ScopedFieldTrials field_trials(
      "WebRTC-KeyframeInterval/max_wait_for_received_frame_ms:100/");
  EXPECT_EQ(KeyframeIntervalSettings::ParseFromFieldTrials()
                .MaxWaitForReceivedFrameMs(),
            100);
}

TEST(KeyframeIntervalSettingsTest, AllValuesSet) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-KeyframeInterval/"
      "min_keyframe_send_interval_ms:100,"
      "max_wait_for_received_keyframe_ms:101,"
      "max_wait_for_received_frame_ms:102/");
  EXPECT_EQ(KeyframeIntervalSettings::ParseFromFieldTrials()
                .MinKeyframeSendIntervalMs(),
            100);
  EXPECT_EQ(KeyframeIntervalSettings::ParseFromFieldTrials()
                .MaxWaitForReceivedKeyframeMs(),
            101);
  EXPECT_EQ(KeyframeIntervalSettings::ParseFromFieldTrials()
                .MaxWaitForReceivedFrameMs(),
            102);
}

TEST(KeyframeIntervalSettingsTest, AllValuesIncorrectlySet) {
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MaxWaitForReceivedFrameMs());

  test::ScopedFieldTrials field_trials(
      "WebRTC-KeyframeInterval/"
      "min_keyframe_send_interval_ms:a,"
      "max_wait_for_received_keyframe_ms:b,"
      "max_wait_for_received_frame_ms:c/");
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MinKeyframeSendIntervalMs());
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MaxWaitForReceivedKeyframeMs());
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MaxWaitForReceivedFrameMs());
}

}  // namespace
}  // namespace webrtc
