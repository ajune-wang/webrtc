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

#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {

TEST(RttMultExperimentTest, RttMultEnabledByDefault) {
  EXPECT_TRUE(RttMultExperiment::RttMultEnabled());
}

TEST(RttMultExperimentTest, RttMultDisabledByFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials("WebRTC-RttMult/Disabled/");
  EXPECT_FALSE(RttMultExperiment::RttMultEnabled());
}

}  // namespace webrtc
