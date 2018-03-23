/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_agc.h"

#include <algorithm>
#include <numeric>

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/audio_processing/vad/voice_activity_detector.h"

namespace webrtc {
namespace {
float FrameEnergy(AudioFrameView<const float> audio) {
  float energy = 0.f;
  for (size_t k = 0; k < audio.num_channels(); ++k) {
    float channel_energy =
        std::accumulate(audio.channel(k).begin(), audio.channel(k).end(), 0.f,
                        [](float a, float b) -> float { return a + b * b; });
    energy = std::max(channel_energy, energy);
  }
  return energy;
}

// Energy as sum of squares
float EnergyToDbfs(float signal_energy, size_t num_samples) {
  float rms = std::pow(signal_energy, 0.5f) / num_samples;
  return FloatS16ToDbfs(rms);
}
}  // namespace

AdaptiveAgc::AdaptiveAgc(ApmDataDumper* apm_data_dumper)
    : speech_level_estimator_(apm_data_dumper),
      gain_applier_(apm_data_dumper),
      apm_data_dumper_(apm_data_dumper) {
  RTC_DCHECK(apm_data_dumper);
}

AdaptiveAgc::~AdaptiveAgc() = default;

void AdaptiveAgc::Process(AudioFrameView<float> float_frame) {
  // The VAD produces 3 estimates every 3rd frame. We want to feed all
  // to the level estimator, but only care about the last level it produces.
  rtc::ArrayView<const VadWithLevel::LevelAndProbability> vad_results =
      vad_.AnalyzeFrame(float_frame);
  for (const auto& vad_result : vad_results) {
    apm_data_dumper_->DumpRaw("agc2_vad_probability",
                              vad_result.speech_probability);
    apm_data_dumper_->DumpRaw("agc2_vad_rms_dbfs", vad_result.speech_rms_dbfs);

    apm_data_dumper_->DumpRaw("agc2_vad_peak_dbfs",
                              vad_result.speech_peak_dbfs);
    speech_level_estimator_.EstimateLevel(vad_result);
  }

  const float speech_level_dbfs = speech_level_estimator_.LatestLevelEstimate();

  // TODO(webrtc.org/???): compute whether frame is noise.
  const bool is_noise = true;

  // Cast to 'float' for easier plotting, because all other dumps are
  // float.
  apm_data_dumper_->DumpRaw("agc2_signal_type_is_noise",
                            static_cast<float>(is_noise));

  const float noise_level_dbfs = EnergyToDbfs(
      noise_level_estimator_.Analyze(is_noise, FrameEnergy(float_frame)),
      float_frame.samples_per_channel());

  apm_data_dumper_->DumpRaw("agc2_noise_estimate_dbfs", noise_level_dbfs);

  // Now apply the gain!
  gain_applier_.Process(speech_level_dbfs, noise_level_dbfs, vad_results,
                        float_frame);
}

}  // namespace webrtc
