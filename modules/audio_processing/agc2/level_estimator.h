/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_LEVEL_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AGC2_LEVEL_ESTIMATOR_H_

#include <array>
#include <vector>

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/include/float_audio_frame.h"

namespace webrtc {

class ApmDataDumper;
namespace agc2 {
class LevelEstimator {
 public:
  LevelEstimator(size_t sample_rate_hz, ApmDataDumper* apm_data_dumper);

  // The input level need not be normalized; the operation of LevelEstimator
  // is invariant under scaling of the input level.
  std::array<float, kSubFramesInFrame> ComputeLevel(
      const FloatAudioFrame& float_frame);

  // Rate may be changed at any time from the value passed to the
  // constructor.
  void SetSampleRate(size_t sample_rate_hz);

 private:
  void CheckParameterCombination();

  float attack_filter_constant_;
  float decay_filter_constant_;

  float filter_state_level_ = 0.f;

  size_t samples_in_frame_;
  size_t samples_in_sub_frame_;

  ApmDataDumper* const apm_data_dumper_;
};
}  // namespace agc2
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_LEVEL_ESTIMATOR_H_
