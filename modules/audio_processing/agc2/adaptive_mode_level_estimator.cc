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

bool AdaptiveModeLevelEstimator::State::operator==(
    const AdaptiveModeLevelEstimator::State& b) const {
  return time_to_full_buffer_ms == b.time_to_full_buffer_ms &&
         level_dbfs.numerator == b.level_dbfs.numerator &&
         level_dbfs.denominator == b.level_dbfs.denominator &&
         saturation_protector == b.saturation_protector;
}

float AdaptiveModeLevelEstimator::State::Ratio::GetRatio() const {
  RTC_DCHECK_NE(denominator, 0.f);
  return numerator / denominator;
}

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper)
    : AdaptiveModeLevelEstimator(
          apm_data_dumper,
          AudioProcessing::Config::GainController2::LevelEstimator::kRms,
          kDefaultMinConsecutiveSpeechFrames,
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
                                 kDefaultMinConsecutiveSpeechFrames,
                                 use_saturation_protector,
                                 kDefaultInitialSaturationMarginDb,
                                 extra_saturation_margin_db) {}

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper,
    AudioProcessing::Config::GainController2::LevelEstimator level_estimator,
    int min_consecutive_speech_frames,
    bool use_saturation_protector,
    float initial_saturation_margin_db,
    float extra_saturation_margin_db)
    : apm_data_dumper_(apm_data_dumper),
      level_estimator_type_(level_estimator),
      min_consecutive_speech_frames_(min_consecutive_speech_frames),
      use_saturation_protector_(use_saturation_protector),
      initial_saturation_margin_db_(initial_saturation_margin_db),
      extra_saturation_margin_db_(extra_saturation_margin_db),
      last_level_dbfs_(absl::nullopt) {
  Reset();
}

void AdaptiveModeLevelEstimator::Update(
    const VadWithLevel::LevelAndProbability& vad_data) {
  RTC_DCHECK_GT(vad_data.speech_rms_dbfs, -150.f);
  RTC_DCHECK_LT(vad_data.speech_rms_dbfs, 50.f);
  RTC_DCHECK_GT(vad_data.speech_peak_dbfs, -150.f);
  RTC_DCHECK_LT(vad_data.speech_peak_dbfs, 50.f);
  RTC_DCHECK_GE(vad_data.speech_probability, 0.f);
  RTC_DCHECK_LE(vad_data.speech_probability, 1.f);

  if (vad_data.speech_probability < kVadConfidenceThreshold) {
    // Not a speech frame.
    if (num_adjacent_speech_frames_ > 0) {
      // First non-speech frame after speech.
      num_adjacent_speech_frames_ = 0;
      if (min_consecutive_speech_frames_ > 1) {
        temporary_state_ = reliable_state_;
      }
    } else if (min_consecutive_speech_frames_ > 1) {
      RTC_DCHECK(temporary_state_ == reliable_state_);
    }
    DumpDebugData();
    return;
  }

  // Speech frame observed.
  num_adjacent_speech_frames_++;

  if (num_adjacent_speech_frames_ < min_consecutive_speech_frames_) {
    // The current frame is a speech candidate, but we still have not observed
    // enough adjacent speech frames. Hence, update the temporary level
    // estimation.
    UpdateState(vad_data, temporary_state_);
    DumpDebugData();
    return;
  }
  if (num_adjacent_speech_frames_ == min_consecutive_speech_frames_ &&
      min_consecutive_speech_frames_ > 1) {
    // Enough adjacent speech frames observed; hence, the temporary estimation
    // is now reliable.
    reliable_state_ = temporary_state_;
  }
  UpdateState(vad_data, reliable_state_);

  // Cache the last reliable level estimation.
  last_level_dbfs_ = reliable_state_.level_dbfs.GetRatio();

  DumpDebugData();
}

float AdaptiveModeLevelEstimator::GetLevelDbfs() const {
  float level_dbfs = last_level_dbfs_.has_value()
                         ? last_level_dbfs_.value()
                         : kInitialSpeechLevelEstimateDbfs;
  if (use_saturation_protector_) {
    level_dbfs += reliable_state_.saturation_protector.margin_db;
    level_dbfs += extra_saturation_margin_db_;
  }
  return rtc::SafeClamp<float>(level_dbfs, -90.f, 30.f);
}

bool AdaptiveModeLevelEstimator::IsConfident() const {
  return reliable_state_.time_to_full_buffer_ms == 0;
}

void AdaptiveModeLevelEstimator::UpdateState(
    const VadWithLevel::LevelAndProbability& vad_data,
    State& state) {
  const bool buffer_is_full = state.time_to_full_buffer_ms == 0;
  if (!buffer_is_full) {
    state.time_to_full_buffer_ms -= kFrameDurationMs;
  }

  // Read level estimation.
  float level_dbfs = 0.f;
  using LevelEstimatorType =
      AudioProcessing::Config::GainController2::LevelEstimator;
  switch (level_estimator_type_) {
    case LevelEstimatorType::kRms:
      level_dbfs = vad_data.speech_rms_dbfs;
      break;
    case LevelEstimatorType::kPeak:
      level_dbfs = vad_data.speech_peak_dbfs;
      break;
  }

  // Update level estimation (average level weighted by speech probability).
  RTC_DCHECK_GT(vad_data.speech_probability, 0.f);
  const float leak_factor = buffer_is_full ? kFullBufferLeakFactor : 1.f;
  state.level_dbfs.numerator = state.level_dbfs.numerator * leak_factor +
                               level_dbfs * vad_data.speech_probability;
  state.level_dbfs.denominator =
      state.level_dbfs.denominator * leak_factor + vad_data.speech_probability;

  if (use_saturation_protector_) {
    UpdateSaturationProtectorState(
        /*speech_peak_dbfs=*/vad_data.speech_peak_dbfs,
        /*speech_level_dbfs=*/state.level_dbfs.GetRatio(),
        state.saturation_protector);
  }
}

void AdaptiveModeLevelEstimator::Reset() {
  ResetState(temporary_state_);
  ResetState(reliable_state_);
  last_level_dbfs_ = absl::nullopt;
  num_adjacent_speech_frames_ = 0;
}

void AdaptiveModeLevelEstimator::ResetState(State& state) {
  state.time_to_full_buffer_ms = kFullBufferSizeMs;
  state.level_dbfs.numerator = 0.f;
  state.level_dbfs.denominator = 0.f;
  ResetSaturationProtectorState(initial_saturation_margin_db_,
                                state.saturation_protector);
}

void AdaptiveModeLevelEstimator::DumpDebugData() {
  if (apm_data_dumper_) {
    apm_data_dumper_->DumpRaw("agc2_adaptive_level_estimate_dbfs",
                              GetLevelDbfs());
    apm_data_dumper_->DumpRaw("agc2_adaptive_saturation_margin_db",
                              reliable_state_.saturation_protector.margin_db);
  }
}

}  // namespace webrtc
