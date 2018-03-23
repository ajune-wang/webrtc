/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/noise_level_estimator.h"

#include <algorithm>

//  #include "modules/audio_processing/audio_buffer.h"
// #include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

float NoiseLevelEstimator::Analyze(bool is_noise, float frame_energy) {
  return 1000.f;
}

}  // namespace webrtc
