/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "system_wrappers/include/field_trial.h"

#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace field_trial {
#ifdef GTEST_HAS_DEATH_TEST
#ifdef RTC_DCHECK_IS_ON
#ifndef WEBRTC_EXCLUDE_FIELD_TRIAL_DEFAULT
TEST(FieldTrialValidationTest, AcceptsValidInputs) {
  InitFieldTrialsFromString("");
  InitFieldTrialsFromString("Audio/Enabled/");
  InitFieldTrialsFromString("Audio/Enabled/Video/Disabled/");

  // Duplicate trials with the same value is fine
  InitFieldTrialsFromString("Audio/Enabled/Audio/Enabled/");
  InitFieldTrialsFromString("Audio/Enabled/B/C/Audio/Enabled/");
}

TEST(FieldTrialValidationTest, RejectsBadInputs) {
  // Bad delimiters
  EXPECT_DEATH(InitFieldTrialsFromString("Audio/EnabledVideo/Disabled/"),
               "Invalid field trials string:");
  EXPECT_DEATH(InitFieldTrialsFromString("Audio/Enabled//Video/Disabled/"),
               "Invalid field trials string:");
  EXPECT_DEATH(InitFieldTrialsFromString("/Audio/Enabled/Video/Disabled/"),
               "Invalid field trials string:");
  EXPECT_DEATH(InitFieldTrialsFromString("Audio/Enabled/Video/Disabled"),
               "Invalid field trials string:");
  EXPECT_DEATH(
      InitFieldTrialsFromString("Audio/Enabled/Video/Disabled/garbage"),
      "Invalid field trials string:");

  // Duplicate trials with the same value is not fine
  EXPECT_DEATH(InitFieldTrialsFromString("Audio/Enabled/Audio/Disabled/"),
               "Invalid field trials string:");
  EXPECT_DEATH(InitFieldTrialsFromString("Audio/Enabled/B/C/Audio/Disabled/"),
               "Invalid field trials string:");
}
#endif  // WEBRTC_EXCLUDE_FIELD_TRIAL_DEFAULT
#endif  // RTC_DCHECK_IS_ON
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace field_trial
}  // namespace webrtc
