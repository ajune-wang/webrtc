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

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper)
    : AdaptiveModeLevelEstimator(
          apm_data_dumper,
          GetInitialSaturationMarginDb(),
          AudioProcessing::Config::GainController2::LevelEstimator::kRms,
          GetMinConsecutiveSpeechFrames(),
          /*use_saturation_protector=*/true,
          GetExtraSaturationMarginOffsetDb()) {}

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper,
    AudioProcessing::Config::GainController2::LevelEstimator level_estimator,
    bool use_saturation_protector,
    float extra_saturation_margin_db)
    : AdaptiveModeLevelEstimator(apm_data_dumper,
                                 GetInitialSaturationMarginDb(),
                                 level_estimator,
                                 GetMinConsecutiveSpeechFrames(),
                                 use_saturation_protector,
                                 extra_saturation_margin_db) {}

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper,
    float initial_saturation_margin_db,
    AudioProcessing::Config::GainController2::LevelEstimator level_estimator,
    int min_consecutive_speech_frames,
    bool use_saturation_protector,
    float extra_saturation_margin_db)
    : apm_data_dumper_(apm_data_dumper),
      saturation_protector_(apm_data_dumper, initial_saturation_margin_db),
      level_estimator_type_(level_estimator),
      min_consecutive_speech_frames_(min_consecutive_speech_frames),
      use_saturation_protector_(use_saturation_protector),
      extra_saturation_margin_db_(extra_saturation_margin_db) {}

bool AdaptiveModeLevelEstimator::IsConfident() const {
  return last_state_.time_to_full_buffer_ms == 0;
}

float AdaptiveModeLevelEstimator::GetLevelDbfs() const {
  float level_dbfs = last_level_dbfs_.has_value()
                         ? last_level_dbfs_.value()
                         : kInitialSpeechLevelEstimateDbfs;
  if (use_saturation_protector_) {
    level_dbfs += saturation_protector_.margin_db();
    level_dbfs += extra_saturation_margin_db_;
  }
  return rtc::SafeClamp<float>(level_dbfs, -90.f, 30.f);
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
      // Reset state to the last reliable state.
      temporary_state_ = last_state_;
      num_adjacent_speech_frames_ = 0;
    } else {
      RTC_DCHECK(temporary_state_ == last_state_);
    }
    DebugDumpEstimate();
    return;
  }

  // Speech frame observed.
  num_adjacent_speech_frames_++;

  if (num_adjacent_speech_frames_ < min_consecutive_speech_frames_) {
    // The current frame is a speech candidate; but we still have not observed
    // enough adjacent speech frames. Hence, update the temporary level
    // estimation.
    UpdateState(vad_data, temporary_state_);
    DebugDumpEstimate();
    return;
  }
  if (num_adjacent_speech_frames_ == min_consecutive_speech_frames_) {
    // Enough adjacent speech frames observed; hence, the temporary estimation
    // is now reliable.
    last_state_ = temporary_state_;
  }
  UpdateState(vad_data, last_state_);

  // Cache the last reliable level estimation.
  RTC_DCHECK(last_state_.level_dbfs.has_value());
  last_level_dbfs_ = last_state_.level_dbfs->GetRatio();
  DebugDumpEstimate();
}

void AdaptiveModeLevelEstimator::Reset() {
  ResetState(temporary_state_);
  ResetState(last_state_);
  last_level_dbfs_ = absl::nullopt;
  num_adjacent_speech_frames_ = 0;
  saturation_protector_.Reset();
}

bool AdaptiveModeLevelEstimator::State::operator==(
    const AdaptiveModeLevelEstimator::State& b) const {
  // TODO(crbug.com/webrtc/7494): Also compare saturation protector state.
  return time_to_full_buffer_ms == b.time_to_full_buffer_ms &&
         level_dbfs.has_value() == b.level_dbfs.has_value() &&
         (!level_dbfs.has_value() ||
          (level_dbfs.value().numerator == b.level_dbfs.value().numerator &&
           level_dbfs.value().denominator == b.level_dbfs.value().denominator));
}

bool AdaptiveModeLevelEstimator::State::operator!=(
    const AdaptiveModeLevelEstimator::State& b) const {
  return !(*this == b);
}

float AdaptiveModeLevelEstimator::State::Ratio::GetRatio() const {
  RTC_DCHECK_NE(denominator, 0.f);
  return numerator / denominator;
}

void AdaptiveModeLevelEstimator::ResetState(State& state) {
  state.time_to_full_buffer_ms = kFullBufferSizeMs;
  state.level_dbfs = absl::nullopt;
  // TODO(crbug.com/webrtc/7494): Reset saturation protector state in `state`.
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
  if (!state.level_dbfs.has_value()) {
    state.level_dbfs = {/*numerator=*/level_dbfs * vad_data.speech_probability,
                        /*denominator=*/vad_data.speech_probability};
  } else {
    const float leak_factor = buffer_is_full ? kFullBufferLeakFactor : 1.f;
    state.level_dbfs->numerator = state.level_dbfs->numerator * leak_factor +
                                  level_dbfs * vad_data.speech_probability;
    state.level_dbfs->denominator =
        state.level_dbfs->denominator * leak_factor +
        vad_data.speech_probability;
  }

  // TODO(crbug.com/webrtc/7494): Update saturation protector state in `state`.
  if (use_saturation_protector_) {
    RTC_DCHECK_EQ(min_consecutive_speech_frames_, 1)
        << "Combination of saturation protector with minimum consecutive "
        << "speech frames not implemented yet.";
    saturation_protector_.UpdateMargin(
        /*speech_peak_dbfs=*/vad_data.speech_peak_dbfs,
        /*speech_level_dbfs=*/state.level_dbfs->GetRatio());
  }
}

void AdaptiveModeLevelEstimator::DebugDumpEstimate() {
  if (apm_data_dumper_) {
    apm_data_dumper_->DumpRaw("agc2_adaptive_level_estimate_dbfs",
                              GetLevelDbfs());
  }
  saturation_protector_.DebugDumpEstimate();
}
}  // namespace webrtc
