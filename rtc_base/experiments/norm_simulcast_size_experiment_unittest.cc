/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/norm_simulcast_size_experiment.h"

#include "rtc_base/gunit.h"
#include "test/field_trial.h"

namespace webrtc {

TEST(NormSimulcastSizeExperimentTest, DisabledByDefault) {
  EXPECT_FALSE(NormSimulcastSizeExperiment::Enabled());
}

TEST(NormSimulcastSizeExperimentTest, EnabledWithFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastSize/Enabled-1/");
  EXPECT_TRUE(NormSimulcastSizeExperiment::Enabled());
}

TEST(NormSimulcastSizeExperimentTest, GetExponent) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastSize/Enabled-2/");
  EXPECT_EQ(2, NormSimulcastSizeExperiment::GetBase2Exponent());
}

TEST(NormSimulcastSizeExperimentTest, GetExponentFailsIfNotEnabled) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastSize/Disabled/");
  EXPECT_FALSE(NormSimulcastSizeExperiment::GetBase2Exponent());
}

TEST(NormSimulcastSizeExperimentTest, GetExponentFailsForInvalidFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastSize/Enabled-invalid/");
  EXPECT_FALSE(NormSimulcastSizeExperiment::GetBase2Exponent());
}

TEST(NormSimulcastSizeExperimentTest,
     GetExponentFailsForNegativeOutOfBoundValue) {
  // Supported range: [0, 5].
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastSize/Enabled--1/");
  EXPECT_FALSE(NormSimulcastSizeExperiment::GetBase2Exponent());
}

TEST(NormSimulcastSizeExperimentTest,
     GetExponentFailsForPositiveOutOfBoundValue) {
  // Supported range: [0, 5].
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastSize/Enabled-6/");
  EXPECT_FALSE(NormSimulcastSizeExperiment::GetBase2Exponent());
}

}  // namespace webrtc
