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
TEST(RttMultExperimentTest, RttMultDisabledByDefault) {
  webrtc::test::ScopedFieldTrials field_trials("");
  EXPECT_FALSE(RttMultExperiment::RttMultEnabled());
}

TEST(RttMultExperimentTest, RttMultEnabledByFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-RttMult/Enabled,RttMult/");
  EXPECT_TRUE(RttMultExperiment::RttMultEnabled());
}
}  // namespace webrtc
