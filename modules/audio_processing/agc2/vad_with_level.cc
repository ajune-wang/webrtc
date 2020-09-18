/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/vad_with_level.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "api/array_view.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"

namespace webrtc {
namespace {

float ProcessForPeak(AudioFrameView<const float> frame) {
  float current_max = 0;
  for (const auto& x : frame.channel(0)) {
    current_max = std::max(std::fabs(x), current_max);
  }
  return current_max;
}

float ProcessForRms(AudioFrameView<const float> frame) {
  float rms = 0;
  for (const auto& x : frame.channel(0)) {
    rms += x * x;
  }
  return std::sqrt(rms / frame.samples_per_channel());
}

float SmoothedVadProbability(float p_old, float p_new) {
  if (p_new < p_old) {
    // Instant decay.
    return p_new;
  } else {
    // Attack phase.
    return kSmoothVadProbabilityAttack * p_new +
           (1.f - kSmoothVadProbabilityAttack) * p_old;
  }
}

}  // namespace

VadWithLevel::VadWithLevel() : vad_probability_(0.f) {}
VadWithLevel::~VadWithLevel() = default;

VadWithLevel::LevelAndProbability VadWithLevel::AnalyzeFrame(
    AudioFrameView<const float> frame) {
  SetSampleRate(static_cast<int>(frame.samples_per_channel() * 100));
  std::array<float, rnn_vad::kFrameSize10ms24kHz> work_frame;
  // Feed the 1st channel to the resampler.
  resampler_.Resample(frame.channel(0).data(), frame.samples_per_channel(),
                      work_frame.data(), rnn_vad::kFrameSize10ms24kHz);

  std::array<float, rnn_vad::kFeatureVectorSize> feature_vector;

  const bool is_silence = features_extractor_.CheckSilenceComputeFeatures(
      work_frame, feature_vector);
  const float vad_probability =
      rnn_vad_.ComputeVadProbability(feature_vector, is_silence);
  vad_probability_ = SmoothedVadProbability(
      /*p_old=*/vad_probability_, /*p_new=*/vad_probability);
  return LevelAndProbability(vad_probability_,
                             FloatS16ToDbfs(ProcessForRms(frame)),
                             FloatS16ToDbfs(ProcessForPeak(frame)));
}

void VadWithLevel::SetSampleRate(int sample_rate_hz) {
  // The source number of channels in 1, because we always use the 1st
  // channel.
  resampler_.InitializeIfNeeded(sample_rate_hz, rnn_vad::kSampleRate24kHz,
                                1 /* num_channels */);
}

}  // namespace webrtc
