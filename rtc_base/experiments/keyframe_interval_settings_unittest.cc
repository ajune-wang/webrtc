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

TEST(KeyframeIntervalSettingsTest, MinKeyFrameRequestIntervalMsSet) {
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MinKeyFrameRequestIntervalMs());

  test::ScopedFieldTrials field_trials(
      "WebRTC-KeyframeInterval/min_keyframe_request_interval_ms:100/");
  EXPECT_EQ(KeyframeIntervalSettings::ParseFromFieldTrials()
                .MinKeyFrameRequestIntervalMs(),
            100);
}

TEST(KeyframeIntervalSettingsTest, MaxWaitForKeyframeMsSet) {
  EXPECT_FALSE(
      KeyframeIntervalSettings::ParseFromFieldTrials().MaxWaitForKeyframeMs());

  test::ScopedFieldTrials field_trials(
      "WebRTC-KeyframeInterval/max_wait_for_keyframe_ms:100/");
  EXPECT_EQ(
      KeyframeIntervalSettings::ParseFromFieldTrials().MaxWaitForKeyframeMs(),
      100);
}

TEST(KeyframeIntervalSettingsTest, MaxWaitForFrameMsSet) {
  EXPECT_FALSE(
      KeyframeIntervalSettings::ParseFromFieldTrials().MaxWaitForFrameMs());

  test::ScopedFieldTrials field_trials(
      "WebRTC-KeyframeInterval/max_wait_for_frame_ms:100/");
  EXPECT_EQ(
      KeyframeIntervalSettings::ParseFromFieldTrials().MaxWaitForFrameMs(),
      100);
}

TEST(KeyframeIntervalSettingsTest, AllValuesSet) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-KeyframeInterval/"
      "min_keyframe_request_interval_ms:100,"
      "max_wait_for_keyframe_ms:101,"
      "max_wait_for_frame_ms:102/");
  EXPECT_EQ(KeyframeIntervalSettings::ParseFromFieldTrials()
                .MinKeyFrameRequestIntervalMs(),
            100);
  EXPECT_EQ(
      KeyframeIntervalSettings::ParseFromFieldTrials().MaxWaitForKeyframeMs(),
      101);
  EXPECT_EQ(
      KeyframeIntervalSettings::ParseFromFieldTrials().MaxWaitForFrameMs(),
      102);
}

TEST(KeyframeIntervalSettingsTest, AllValuesIncorrect) {
  EXPECT_FALSE(
      KeyframeIntervalSettings::ParseFromFieldTrials().MaxWaitForFrameMs());

  test::ScopedFieldTrials field_trials(
      "WebRTC-KeyframeInterval/"
      "min_keyframe_request_interval_ms:a,"
      "max_wait_for_keyframe_ms:b,"
      "max_wait_for_frame_ms:c/");
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MinKeyFrameRequestIntervalMs());
  EXPECT_FALSE(
      KeyframeIntervalSettings::ParseFromFieldTrials().MaxWaitForKeyframeMs());
  EXPECT_FALSE(
      KeyframeIntervalSettings::ParseFromFieldTrials().MaxWaitForFrameMs());
}

}  // namespace
}  // namespace webrtc
