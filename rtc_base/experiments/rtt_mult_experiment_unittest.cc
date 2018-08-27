/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/rtt_mult_experiment.h"
#include "rtc_base/gunit.h"
#include "test/field_trial.h"

namespace webrtc {

// Test field trial disabled.
TEST(RttMultExperimentTest, RttMultDisabledByDefault) {
  webrtc::test::ScopedFieldTrials field_trials("");
  EXPECT_FALSE(RttMultExperiment::RttMultEnabled());
}

// Test field trial enabled.
TEST(RttMultExperimentTest, RttMultEnabledByFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials("WebRTC-RttMult/Enabled-0.25/");
  EXPECT_TRUE(RttMultExperiment::RttMultEnabled());
}

// Test getting rtt_mult value from field trial.
TEST(RttMultExperimentTest, RttMultTestValue) {
  webrtc::test::ScopedFieldTrials field_trials("WebRTC-RttMult/Enabled-0.25/");
  EXPECT_EQ(0.25, RttMultExperiment::GetRttMultValue());
}

// Test rtt_mult value out of bounds (low).
TEST(RttMultExperimentTest, RttMultTestValueOutOfBounds) {
  webrtc::test::ScopedFieldTrials field_trials("WebRTC-RttMult/Enable--0.5/");
  EXPECT_EQ(0.0, RttMultExperiment::GetRttMultValue());
}

// Test malformed field trial string (misspelled Enabled).
TEST(RttMultExperimentTest, RttMultTestMalformedEnabled) {
  webrtc::test::ScopedFieldTrials field_trials("WebRTC-RttMult/Enable-0.25/");
  EXPECT_FALSE(RttMultExperiment::RttMultEnabled());
}

// Test malformed field trial string (malformed rtt_mult value).
TEST(RttMultExperimentTest, RttMultTestMalformedValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-RttMult/Enabled-0.25.4/");
  EXPECT_NE(0.25, RttMultExperiment::GetRttMultValue());
}

}  // namespace webrtc
