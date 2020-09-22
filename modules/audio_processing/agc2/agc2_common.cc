/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/agc2_common.h"

#include <stdio.h>

#include <string>

#include "absl/types/optional.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

absl::optional<float> GetFloatFieldTrial(const char* name,
                                         float min,
                                         float max) {
  if (webrtc::field_trial::IsEnabled(name)) {
    const std::string field_trial_value =
        webrtc::field_trial::FindFullName(name);
    float value;
    if (sscanf(field_trial_value.c_str(), "Enabled-%f", &value) == 1 &&
        value >= min && value <= max) {
      return value;
    }
  }
  return absl::nullopt;
}

}  // namespace

float GetSmoothedVadProbabilityAttack() {
  return GetFloatFieldTrial(
             "WebRTC-Audio-Agc2ForceSmoothedVadProbabilityAttack", /*min=*/0.f,
             /*max=*/1.f)
      .value_or(1.f);
}

float GetInitialSaturationMarginDb() {
  return GetFloatFieldTrial("WebRTC-Audio-Agc2ForceInitialSaturationMargin",
                            /*min=*/12.f, /*max=*/25.f)
      .value_or(20.f);
}

float GetExtraSaturationMarginOffsetDb() {
  return GetFloatFieldTrial("WebRTC-Audio-Agc2ForceExtraSaturationMargin",
                            /*min=*/0.f, /*max=*/10.f)
      .value_or(2.f);
}

}  // namespace webrtc
