/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_mode_level_estimator.h"

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

// Combines a level estimation with the saturation protector margins.
float ComputeLevelEstimateDbfs(float level_estimate_dbfs,
                               bool use_saturation_protector,
                               float saturation_margin_db,
                               float extra_saturation_margin_db) {
  return rtc::SafeClamp<float>(
      level_estimate_dbfs +
          (use_saturation_protector
               ? (saturation_margin_db + extra_saturation_margin_db)
               : 0.f),
      -90.f, 30.f);
}

}  // namespace

bool AdaptiveModeLevelEstimator::LevelEstimatorState::operator==(
    const AdaptiveModeLevelEstimator::LevelEstimatorState& b) const {
  return time_to_full_buffer_ms == b.time_to_full_buffer_ms &&
         level_dbfs.numerator == b.level_dbfs.numerator &&
         level_dbfs.denominator == b.level_dbfs.denominator &&
         saturation_protector == b.saturation_protector;
}

float AdaptiveModeLevelEstimator::LevelEstimatorState::Ratio::GetRatio() const {
  RTC_DCHECK_NE(denominator, 0.f);
  return numerator / denominator;
}

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper)
    : AdaptiveModeLevelEstimator(
          apm_data_dumper,
          AudioProcessing::Config::GainController2::LevelEstimator::kRms,
          kDefaultAdjacentSpeechFramesThreshold,
          kDefaultUseSaturationProtector,
          kDefaultInitialSaturationMarginDb,
          kDefaultExtraSaturationMarginDb) {}

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper,
    AudioProcessing::Config::GainController2::LevelEstimator level_estimator,
    bool use_saturation_protector,
    float extra_saturation_margin_db)
    : AdaptiveModeLevelEstimator(apm_data_dumper,
                                 level_estimator,
                                 kDefaultAdjacentSpeechFramesThreshold,
                                 use_saturation_protector,
                                 kDefaultInitialSaturationMarginDb,
                                 extra_saturation_margin_db) {}

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper,
    AudioProcessing::Config::GainController2::LevelEstimator level_estimator,
    int adjacent_speech_frames_threshold,
    bool use_saturation_protector,
    float initial_saturation_margin_db,
    float extra_saturation_margin_db)
    : apm_data_dumper_(apm_data_dumper),
      level_estimator_type_(level_estimator),
      adjacent_speech_frames_threshold_(adjacent_speech_frames_threshold),
      use_saturation_protector_(use_saturation_protector),
      initial_saturation_margin_db_(initial_saturation_margin_db),
      extra_saturation_margin_db_(extra_saturation_margin_db),
      last_level_dbfs_(ComputeLevelEstimateDbfs(kInitialSpeechLevelEstimateDbfs,
                                                use_saturation_protector_,
                                                initial_saturation_margin_db_,
                                                extra_saturation_margin_db_)) {
  RTC_DCHECK(apm_data_dumper_);
  RTC_DCHECK_GE(adjacent_speech_frames_threshold_, 1);
  Reset();
}

void AdaptiveModeLevelEstimator::Update(
    const VadLevelAnalyzer::Result& vad_level) {
  RTC_DCHECK_GT(vad_level.rms_dbfs, -150.f);
  RTC_DCHECK_LT(vad_level.rms_dbfs, 50.f);
  RTC_DCHECK_GT(vad_level.peak_dbfs, -150.f);
  RTC_DCHECK_LT(vad_level.peak_dbfs, 50.f);
  RTC_DCHECK_GE(vad_level.speech_probability, 0.f);
  RTC_DCHECK_LE(vad_level.speech_probability, 1.f);
  DumpDebugData();

  const bool requires_adjacent_speech_frames =
      adjacent_speech_frames_threshold_ > 1;
  if (vad_level.speech_probability < kVadConfidenceThreshold) {
    // Not a speech frame.
    if (requires_adjacent_speech_frames &&
        num_adjacent_speech_frames_ >= adjacent_speech_frames_threshold_) {
      // First non-speech frame after a long enough sequence of speech frames.
      reliable_state_ = preliminary_state_;
    } else if (requires_adjacent_speech_frames &&
               num_adjacent_speech_frames_ > 0) {
      // First non-speech frame after a too short sequence of speech frames.
      preliminary_state_ = reliable_state_;
    }
    num_adjacent_speech_frames_ = 0;
    return;
  }

  // Speech frame observed.
  num_adjacent_speech_frames_++;

  UpdatePreliminaryLevelEstimatorState(vad_level);

  if (num_adjacent_speech_frames_ >= adjacent_speech_frames_threshold_) {
    // `preliminary_state_` is now reliable. Update the last level estimation.
    last_level_dbfs_ = ComputeLevelEstimateDbfs(
        preliminary_state_.level_dbfs.GetRatio(), use_saturation_protector_,
        preliminary_state_.saturation_protector.margin_db,
        extra_saturation_margin_db_);
  }
}

void AdaptiveModeLevelEstimator::UpdatePreliminaryLevelEstimatorState(
    const VadLevelAnalyzer::Result& vad_level) {
  auto& state = preliminary_state_;

  RTC_DCHECK_GE(state.time_to_full_buffer_ms, 0);
  const bool buffer_is_full = state.time_to_full_buffer_ms == 0;
  if (!buffer_is_full) {
    state.time_to_full_buffer_ms -= kFrameDurationMs;
  }

  // Read level estimation.
  using LevelEstimatorType =
      AudioProcessing::Config::GainController2::LevelEstimator;
  float level_dbfs = 0.f;
  switch (level_estimator_type_) {
    case LevelEstimatorType::kRms:
      level_dbfs = vad_level.rms_dbfs;
      break;
    case LevelEstimatorType::kPeak:
      level_dbfs = vad_level.peak_dbfs;
      break;
  }

  // Update level estimation (average level weighted by speech probability).
  RTC_DCHECK_GT(vad_level.speech_probability, 0.f);
  const float leak_factor = buffer_is_full ? kFullBufferLeakFactor : 1.f;
  state.level_dbfs.numerator = state.level_dbfs.numerator * leak_factor +
                               level_dbfs * vad_level.speech_probability;
  state.level_dbfs.denominator =
      state.level_dbfs.denominator * leak_factor + vad_level.speech_probability;

  if (use_saturation_protector_) {
    UpdateSaturationProtectorState(
        /*speech_peak_dbfs=*/vad_level.peak_dbfs,
        /*speech_level_dbfs=*/state.level_dbfs.GetRatio(),
        state.saturation_protector);
  }
}

void AdaptiveModeLevelEstimator::Reset() {
  ResetLevelEstimatorState(preliminary_state_);
  ResetLevelEstimatorState(reliable_state_);
  last_level_dbfs_ = ComputeLevelEstimateDbfs(
      kInitialSpeechLevelEstimateDbfs, use_saturation_protector_,
      initial_saturation_margin_db_, extra_saturation_margin_db_);
  num_adjacent_speech_frames_ = 0;
}

void AdaptiveModeLevelEstimator::ResetLevelEstimatorState(
    LevelEstimatorState& state) {
  state.time_to_full_buffer_ms = kFullBufferSizeMs;
  state.level_dbfs.numerator = 0.f;
  state.level_dbfs.denominator = 0.f;
  ResetSaturationProtectorState(initial_saturation_margin_db_,
                                state.saturation_protector);
}

void AdaptiveModeLevelEstimator::DumpDebugData() {
  apm_data_dumper_->DumpRaw("agc2_adaptive_level_estimate_dbfs", level_dbfs());
  apm_data_dumper_->DumpRaw("agc2_adaptive_num_adjacent_speech_frames_",
                            num_adjacent_speech_frames_);
  apm_data_dumper_->DumpRaw("agc2_adaptive_preliminary_level_estimate_num",
                            preliminary_state_.level_dbfs.numerator);
  apm_data_dumper_->DumpRaw("agc2_adaptive_preliminary_level_estimate_den",
                            preliminary_state_.level_dbfs.denominator);
  apm_data_dumper_->DumpRaw("agc2_adaptive_preliminary_saturation_margin_db",
                            preliminary_state_.saturation_protector.margin_db);
}

}  // namespace webrtc
