/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <algorithm>
#include <functional>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/reverb_modelling.h"

namespace webrtc {

ReverbModelling::ReverbModelling() {
  Reset();
}

ReverbModelling::~ReverbModelling() = default;

void ReverbModelling::Reset() {
  reverb_.fill(0.);
}

void ReverbModelling::UpdateReverbContributions(
    rtc::ArrayView<const float> tail,
    float gain_tail,
    float reverb_decay) {
  if (reverb_decay > 0) {
    // Update the estimate of the reverberant power.
    std::transform(tail.begin(), tail.end(), reverb_.begin(), reverb_.begin(),
                   [reverb_decay, gain_tail](float a, float b) {
                     return (b + a * gain_tail) * reverb_decay;
                   });
  }
}

void ReverbModelling::AddReverb(
    rtc::ArrayView<const float> tail,
    float gain_tail,
    float reverb_decay,
    std::array<float, kFftLengthBy2Plus1>* power_spectrum) {
  UpdateReverbContributions(tail, gain_tail, reverb_decay);

  // Add the power of the echo reverb to the residual echo power.
  std::transform(power_spectrum->begin(), power_spectrum->end(),
                 reverb_.begin(), power_spectrum->begin(), std::plus<float>());
}

}  // namespace webrtc
