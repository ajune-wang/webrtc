/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/field_trials.h"

#include <memory>

#include "api/transport/field_trial_based_config.h"
#include "system_wrappers/include/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
#include "test/testsupport/rtc_expect_death.h"
#endif  // GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

namespace webrtc {
namespace {

using ::testing::NotNull;

TEST(FieldTrialsTest, EmptyStringHasNoEffect) {
  FieldTrials f("");
  EXPECT_FALSE(f.IsEnabled("TestKey-A"));
  EXPECT_FALSE(f.IsDisabled("TestKey-A"));
}

TEST(FieldTrialsTest, EnabledDisabledMustBeFirstInValue) {
  FieldTrials f(
      "TestKey-A/EnabledFoo/"
      "TestKey-B/DisabledBar/"
      "TestKey-C/BazEnabled/");
  EXPECT_TRUE(f.IsEnabled("TestKey-A"));
  EXPECT_TRUE(f.IsDisabled("TestKey-B"));
  EXPECT_FALSE(f.IsEnabled("TestKey-C"));
}

TEST(FieldTrialsTest, FieldTrialsDoesNotReadGlobalString) {
  static constexpr char s[] = "TestKey-A/Enabled/TestKey-B/Disabled/";
  webrtc::field_trial::InitFieldTrialsFromString(s);
  FieldTrials f("");
  EXPECT_FALSE(f.IsEnabled("TestKey-A"));
  EXPECT_FALSE(f.IsDisabled("TestKey-B"));
}

TEST(FieldTrialsTest, FieldTrialsWritesGlobalString) {
  FieldTrials f("TestKey-A/Enabled/TestKey-B/Disabled/");
  EXPECT_TRUE(webrtc::field_trial::IsEnabled("TestKey-A"));
  EXPECT_TRUE(webrtc::field_trial::IsDisabled("TestKey-B"));
}

TEST(FieldTrialsTest, FieldTrialsRestoresGlobalStringAfterDestruction) {
  static constexpr char s[] = "TestKey-A/Enabled/";
  webrtc::field_trial::InitFieldTrialsFromString(s);
  {
    FieldTrials f("TestKey-B/Enabled/");
    EXPECT_STREQ(webrtc::field_trial::GetFieldTrialString(),
                 "TestKey-B/Enabled/");
  }
  EXPECT_STREQ(webrtc::field_trial::GetFieldTrialString(), s);
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(FieldTrialsTest, FieldTrialsDoesNotSupportSimultaneousInstances) {
  FieldTrials f("TestKey-A/Enabled/");
  RTC_EXPECT_DEATH(FieldTrials("TestKey-B/Enabled/").Lookup("Whatever"),
                   "Only one instance");
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(FieldTrialsTest, FieldTrialsSupportsSeparateInstances) {
  { FieldTrials f("TestKey-A/Enabled/"); }
  { FieldTrials f("TestKey-B/Enabled/"); }
}

TEST(FieldTrialsTest, NonGlobalFieldTrialsInstanceDoesNotModifyGlobalString) {
  std::unique_ptr<FieldTrials> f =
      FieldTrials::CreateNoGlobal("TestKey-A/Enabled/");
  ASSERT_THAT(f, NotNull());
  EXPECT_TRUE(f->IsEnabled("TestKey-A"));
  EXPECT_FALSE(webrtc::field_trial::IsEnabled("TestKey-A"));
}

TEST(FieldTrialsTest, NonGlobalFieldTrialsSupportSimultaneousInstances) {
  std::unique_ptr<FieldTrials> f1 =
      FieldTrials::CreateNoGlobal("TestKey-A/Enabled/");
  std::unique_ptr<FieldTrials> f2 =
      FieldTrials::CreateNoGlobal("TestKey-B/Enabled/");
  ASSERT_THAT(f1, NotNull());
  ASSERT_THAT(f2, NotNull());

  EXPECT_TRUE(f1->IsEnabled("TestKey-A"));
  EXPECT_FALSE(f1->IsEnabled("TestKey-B"));

  EXPECT_FALSE(f2->IsEnabled("TestKey-A"));
  EXPECT_TRUE(f2->IsEnabled("TestKey-B"));
}

TEST(FieldTrialsTest, GlobalAndNonGlobalFieldTrialsAreDisjoint) {
  FieldTrials f1("TestKey-A/Enabled/");
  std::unique_ptr<FieldTrials> f2 =
      FieldTrials::CreateNoGlobal("TestKey-B/Enabled/");
  ASSERT_THAT(f2, NotNull());

  EXPECT_TRUE(f1.IsEnabled("TestKey-A"));
  EXPECT_FALSE(f1.IsEnabled("TestKey-B"));

  EXPECT_FALSE(f2->IsEnabled("TestKey-A"));
  EXPECT_TRUE(f2->IsEnabled("TestKey-B"));
}

TEST(FieldTrialsTest, FieldTrialBasedConfigReadsGlobalString) {
  static constexpr char s[] = "TestKey-A/Enabled/TestKey-B/Disabled/";
  webrtc::field_trial::InitFieldTrialsFromString(s);
  FieldTrialBasedConfig f;
  EXPECT_TRUE(f.IsEnabled("TestKey-A"));
  EXPECT_TRUE(f.IsDisabled("TestKey-B"));
}

}  // namespace
}  // namespace webrtc
