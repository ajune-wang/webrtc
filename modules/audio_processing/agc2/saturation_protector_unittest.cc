/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/saturation_protector.h"

#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

constexpr float kInitialHaedroomDb = 42.0f;
constexpr float kExtraHeadroomDb = 2.0f;
constexpr int kAdjacentSpeechFramesThreshold = 1;
constexpr float kSpeechLowLevel = -55.0f;
constexpr float kSpeechClipping = 0.0f;
constexpr float kMinSpeechProbability = 0.0f;
// constexpr float kMaxSpeechProbability = 1.0f;

TEST(GainController2SaturationProtector, DISABLED_FixedInit) {
  ApmDataDumper apm_data_dumper(0);
  auto saturation_protector = CreateSaturationProtector(
      kInitialHaedroomDb, kExtraHeadroomDb, kAdjacentSpeechFramesThreshold,
      &apm_data_dumper);
  EXPECT_EQ(saturation_protector->HeadroomDb(), kMinSpeechProbability);
}

TEST(GainController2SaturationProtector,
     DISABLED_FixedDoesNotReactToLowLevels) {
  ApmDataDumper apm_data_dumper(0);
  auto saturation_protector = CreateSaturationProtector(
      kInitialHaedroomDb, kExtraHeadroomDb, kAdjacentSpeechFramesThreshold,
      &apm_data_dumper);
  saturation_protector->Analyze(kMinSpeechProbability,
                                /*peak_dbfs=*/kSpeechLowLevel, kSpeechLowLevel);
  EXPECT_EQ(saturation_protector->HeadroomDb(), kMinSpeechProbability);
}

TEST(GainController2SaturationProtector, DISABLED_FixedDoesNotReactToClipping) {
  ApmDataDumper apm_data_dumper(0);
  auto saturation_protector = CreateSaturationProtector(
      kInitialHaedroomDb, kExtraHeadroomDb, kAdjacentSpeechFramesThreshold,
      &apm_data_dumper);
  saturation_protector->Analyze(kMinSpeechProbability,
                                /*peak_dbfs=*/kSpeechClipping, kSpeechClipping);
  EXPECT_EQ(saturation_protector->HeadroomDb(), kMinSpeechProbability);
}

}  // namespace
}  // namespace webrtc
