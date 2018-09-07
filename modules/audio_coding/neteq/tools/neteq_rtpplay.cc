/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/audio_coding/neteq/tools/neteq_test_factory.h"
#include "rtc_base/flags.h"
#include "test/field_trial.h"

DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature.");

int main(int argc, char* argv[]) {
  webrtc::test::NetEqTestFactory factory;
  std::unique_ptr<webrtc::test::NetEqTest> test =
      factory.InitializeTest(argc, argv);
  webrtc::test::ValidateFieldTrialsStringOrDie("");
  webrtc::test::ScopedFieldTrials field_trials(FLAG_force_fieldtrials);
  test->Run();
  return 0;
}
