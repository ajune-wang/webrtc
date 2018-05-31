/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/field_trial_parser.h"
#include <stdio.h>
#include "rtc_base/gunit.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"

namespace webrtc {
namespace {
const char kDummyExperiment[] = "WebRTC-DummyExperiment";

struct DummyExperiment {
  TrialFlagField enabled = TrialFlagField("Enabled");
  TrialField<double> factor = TrialField<double>("f", 0.5);
  TrialField<int64_t> retries = TrialField<int64_t>("r", 5);
  TrialField<bool> ping = TrialField<bool>("p", 0);
  TrialField<std::string> hash = TrialField<std::string>("h", "a80");

  explicit DummyExperiment(std::string field_trial) {
    ParseFieldTrialFields({&enabled, &factor, &retries, &ping, &hash},
                          field_trial);
  }
  DummyExperiment() {
    std::string trial_string = field_trial::FindFullName(kDummyExperiment);
    ParseFieldTrialFields({&enabled, &factor, &retries, &ping, &hash},
                          trial_string);
  }
};
}  // namespace

TEST(FieldTrialParserTest, ParsesValidParameters) {
  DummyExperiment exp("Enabled,f:-1.7,r:2,p:1,h:x7c");
  EXPECT_TRUE(exp.enabled.IsSet());
  EXPECT_EQ(exp.factor.Get(), -1.7);
  EXPECT_EQ(exp.retries.Get(), 2);
  EXPECT_EQ(exp.ping.Get(), true);
  EXPECT_EQ(exp.hash.Get(), "x7c");
}

TEST(FieldTrialParserTest, InitializesFromFieldTrial) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-DummyExperiment/Enabled,f:-1.7,r:2,p:1,h:x7c/");
  DummyExperiment exp;
  EXPECT_TRUE(exp.enabled.IsSet());
  EXPECT_EQ(exp.factor.Get(), -1.7);
  EXPECT_EQ(exp.retries.Get(), 2);
  EXPECT_EQ(exp.ping.Get(), true);
  EXPECT_EQ(exp.hash.Get(), "x7c");
}
TEST(FieldTrialParserTest, UsesDefaults) {
  DummyExperiment exp("");
  EXPECT_FALSE(exp.enabled.IsSet());
  EXPECT_EQ(exp.factor.Get(), 0.5);
  EXPECT_EQ(exp.retries.Get(), 5);
  EXPECT_EQ(exp.ping.Get(), false);
  EXPECT_EQ(exp.hash.Get(), "a80");
}
TEST(FieldTrialParserTest, CanHandleMixedInput) {
  DummyExperiment exp("p:1,h,Enabled");
  EXPECT_TRUE(exp.enabled.IsSet());
  EXPECT_EQ(exp.factor.Get(), 0.5);
  EXPECT_EQ(exp.retries.Get(), 5);
  EXPECT_EQ(exp.ping.Get(), true);
  EXPECT_EQ(exp.hash.Get(), "");
}

TEST(FieldTrialParserTest, IgnoresNewKey) {
  DummyExperiment exp("Disabled,r:-11,foo");
  EXPECT_FALSE(exp.enabled.IsSet());
  EXPECT_EQ(exp.factor.Get(), 0.5);
  EXPECT_EQ(exp.retries.Get(), -11);
}
TEST(FieldTrialParserTest, IgnoresInvalid) {
  DummyExperiment exp("Enabled,f,p:,r:%,,:foo,h");
  EXPECT_TRUE(exp.enabled.IsSet());
  EXPECT_EQ(exp.factor.Get(), 0.5);
  EXPECT_EQ(exp.retries.Get(), 5);
  EXPECT_EQ(exp.ping.Get(), false);
  EXPECT_EQ(exp.hash.Get(), "");
}
}  // namespace webrtc
