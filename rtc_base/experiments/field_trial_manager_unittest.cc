/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/field_trial_manager.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {

TEST(FieldTrialManagerTest, NoTrialsTest) {
  test::ScopedFieldTrials field_trials("");
  DefaultFieldTrialManager field_trial_manager;
  EXPECT_FALSE(field_trial_manager.IsEnabled("Dummy"));
  EXPECT_FALSE(field_trial_manager.IsDisabled("Dummy"));
}

TEST(FieldTrialManagerTest, EnabledTest) {
  test::ScopedFieldTrials field_trials("WebRTC-Dummy/Enabled/");
  DefaultFieldTrialManager field_trial_manager;
  EXPECT_TRUE(field_trial_manager.IsEnabled("Dummy"));
  EXPECT_FALSE(field_trial_manager.IsDisabled("Dummy"));
}

TEST(FieldTrialManagerTest, DisabledTest) {
  test::ScopedFieldTrials field_trials("WebRTC-Dummy/Disabled/");
  DefaultFieldTrialManager field_trial_manager;
  EXPECT_FALSE(field_trial_manager.IsEnabled("Dummy"));
  EXPECT_TRUE(field_trial_manager.IsDisabled("Dummy"));
}

}  // namespace webrtc
