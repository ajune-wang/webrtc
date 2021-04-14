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

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/vad_with_level.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

<<<<<<< HEAD   (48ae01 [Merge-91] Remove RTCRemoteInboundRtpStreamStats duplicate m)
void DumpDebugData(const AdaptiveDigitalGainApplier::FrameInfo& info,
                   ApmDataDumper& dumper) {
  dumper.DumpRaw("agc2_vad_probability", info.vad_result.speech_probability);
  dumper.DumpRaw("agc2_vad_rms_dbfs", info.vad_result.rms_dbfs);
  dumper.DumpRaw("agc2_vad_peak_dbfs", info.vad_result.peak_dbfs);
  dumper.DumpRaw("agc2_noise_estimate_dbfs", info.input_noise_level_dbfs);
  dumper.DumpRaw("agc2_last_limiter_audio_level", info.limiter_envelope_dbfs);
}
=======
using AdaptiveDigitalConfig =
    AudioProcessing::Config::GainController2::AdaptiveDigital;
using NoiseEstimatorType =
    AudioProcessing::Config::GainController2::NoiseEstimator;
>>>>>>> CHANGE (980c46 AGC2: retuning and large refactoring)

constexpr int kGainApplierAdjacentSpeechFramesThreshold = 1;
constexpr float kMaxGainChangePerSecondDb = 3.0f;
constexpr float kMaxOutputNoiseLevelDbfs = -50.0f;

// Detects the available CPU features and applies any kill-switches.
AvailableCpuFeatures GetAllowedCpuFeatures(
    const AudioProcessing::Config::GainController2::AdaptiveDigital& config) {
  AvailableCpuFeatures features = GetAvailableCpuFeatures();
  if (!config.sse2_allowed) {
    features.sse2 = false;
  }
  if (!config.avx2_allowed) {
    features.avx2 = false;
  }
  if (!config.neon_allowed) {
    features.neon = false;
  }
  return features;
}

}  // namespace

AdaptiveAgc::AdaptiveAgc(ApmDataDumper* apm_data_dumper)
    : speech_level_estimator_(apm_data_dumper),
      gain_controller_(apm_data_dumper,
                       kGainApplierAdjacentSpeechFramesThreshold,
                       kMaxGainChangePerSecondDb,
                       kMaxOutputNoiseLevelDbfs),
      apm_data_dumper_(apm_data_dumper),
<<<<<<< HEAD   (48ae01 [Merge-91] Remove RTCRemoteInboundRtpStreamStats duplicate m)
      noise_level_estimator_(apm_data_dumper) {
=======
      noise_level_estimator_(
          CreateNoiseLevelEstimator(kDefaultNoiseLevelEstimatorType,
                                    apm_data_dumper)),
      saturation_protector_(
          CreateSaturationProtector(kSaturationProtectorInitialHeadroomDb,
                                    kSaturationProtectorExtraHeadroomDb,
                                    kGainApplierAdjacentSpeechFramesThreshold,
                                    apm_data_dumper)) {
>>>>>>> CHANGE (980c46 AGC2: retuning and large refactoring)
  RTC_DCHECK(apm_data_dumper);
}

AdaptiveAgc::AdaptiveAgc(ApmDataDumper* apm_data_dumper,
<<<<<<< HEAD   (48ae01 [Merge-91] Remove RTCRemoteInboundRtpStreamStats duplicate m)
                         const AudioProcessing::Config::GainController2& config)
    : speech_level_estimator_(
          apm_data_dumper,
          config.adaptive_digital.level_estimator,
          config.adaptive_digital
              .level_estimator_adjacent_speech_frames_threshold,
          config.adaptive_digital.initial_saturation_margin_db,
          config.adaptive_digital.extra_saturation_margin_db),
      vad_(config.adaptive_digital.vad_reset_period_ms,
           config.adaptive_digital.vad_probability_attack,
           GetAllowedCpuFeatures(config.adaptive_digital)),
      gain_applier_(
          apm_data_dumper,
          config.adaptive_digital.gain_applier_adjacent_speech_frames_threshold,
          config.adaptive_digital.max_gain_change_db_per_second,
          config.adaptive_digital.max_output_noise_level_dbfs),
=======
                         const AdaptiveDigitalConfig& config)
    : speech_level_estimator_(apm_data_dumper,
                              config.adjacent_speech_frames_threshold),
      vad_(config.vad_reset_period_ms, GetAllowedCpuFeatures(config)),
      gain_controller_(apm_data_dumper,
                       config.adjacent_speech_frames_threshold,
                       config.max_gain_change_db_per_second,
                       config.max_output_noise_level_dbfs),
>>>>>>> CHANGE (980c46 AGC2: retuning and large refactoring)
      apm_data_dumper_(apm_data_dumper),
<<<<<<< HEAD   (48ae01 [Merge-91] Remove RTCRemoteInboundRtpStreamStats duplicate m)
      noise_level_estimator_(apm_data_dumper) {
=======
      noise_level_estimator_(
          CreateNoiseLevelEstimator(config.noise_estimator, apm_data_dumper)),
      saturation_protector_(
          CreateSaturationProtector(kSaturationProtectorInitialHeadroomDb,
                                    kSaturationProtectorExtraHeadroomDb,
                                    config.adjacent_speech_frames_threshold,
                                    apm_data_dumper)) {
>>>>>>> CHANGE (980c46 AGC2: retuning and large refactoring)
  RTC_DCHECK(apm_data_dumper);
<<<<<<< HEAD   (48ae01 [Merge-91] Remove RTCRemoteInboundRtpStreamStats duplicate m)
  if (!config.adaptive_digital.use_saturation_protector) {
=======
  RTC_DCHECK(noise_level_estimator_);
  RTC_DCHECK(saturation_protector_);
  if (!config.use_saturation_protector) {
>>>>>>> CHANGE (980c46 AGC2: retuning and large refactoring)
    RTC_LOG(LS_WARNING) << "The saturation protector cannot be disabled.";
  }
}

AdaptiveAgc::~AdaptiveAgc() = default;

void AdaptiveAgc::Process(AudioFrameView<float> frame, float limiter_envelope) {
  AdaptiveDigitalGainApplier::FrameInfo info;
<<<<<<< HEAD   (48ae01 [Merge-91] Remove RTCRemoteInboundRtpStreamStats duplicate m)
  info.vad_result = vad_.AnalyzeFrame(frame);
  speech_level_estimator_.Update(info.vad_result);
  info.input_level_dbfs = speech_level_estimator_.level_dbfs();
  info.input_noise_level_dbfs = noise_level_estimator_.Analyze(frame);
  info.limiter_envelope_dbfs =
      limiter_envelope > 0 ? FloatS16ToDbfs(limiter_envelope) : -90.0f;
  info.estimate_is_confident = speech_level_estimator_.IsConfident();
  DumpDebugData(info, *apm_data_dumper_);
  gain_applier_.Process(info, frame);
=======

  VadLevelAnalyzer::Result vad_result = vad_.AnalyzeFrame(frame);
  info.speech_probability = vad_result.speech_probability;
  apm_data_dumper_->DumpRaw("agc2_speech_probability",
                            vad_result.speech_probability);
  apm_data_dumper_->DumpRaw("agc2_input_rms_dbfs", vad_result.rms_dbfs);
  apm_data_dumper_->DumpRaw("agc2_input_peak_dbfs", vad_result.peak_dbfs);

  speech_level_estimator_.Update(vad_result);
  info.speech_level_dbfs = speech_level_estimator_.level_dbfs();
  info.speech_level_reliable = speech_level_estimator_.IsConfident();
  apm_data_dumper_->DumpRaw("agc2_speech_level_dbfs", info.speech_level_dbfs);
  apm_data_dumper_->DumpRaw("agc2_speech_level_reliable",
                            info.speech_level_reliable);

  info.noise_rms_dbfs = noise_level_estimator_->Analyze(frame);
  apm_data_dumper_->DumpRaw("agc2_noise_rms_dbfs", info.noise_rms_dbfs);

  saturation_protector_->Analyze(info.speech_probability, vad_result.peak_dbfs,
                                 info.speech_level_dbfs);
  info.headroom_db = saturation_protector_->HeadroomDb();
  apm_data_dumper_->DumpRaw("agc2_headroom_db", info.headroom_db);

  info.limiter_envelope_dbfs = FloatS16ToDbfs(limiter_envelope);
  apm_data_dumper_->DumpRaw("agc2_limiter_envelope_dbfs",
                            info.limiter_envelope_dbfs);

  gain_controller_.Process(info, frame);
>>>>>>> CHANGE (980c46 AGC2: retuning and large refactoring)
}

void AdaptiveAgc::HandleInputGainChange() {
  speech_level_estimator_.Reset();
  saturation_protector_->Reset();
}

}  // namespace webrtc
