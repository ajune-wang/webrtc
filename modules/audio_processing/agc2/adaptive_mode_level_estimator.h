/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_MODE_LEVEL_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_MODE_LEVEL_ESTIMATOR_H_

#include <stddef.h>
#include <type_traits>

#include "absl/types/optional.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/saturation_protector.h"
#include "modules/audio_processing/agc2/vad_with_level.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

class ApmDataDumper;

// Level estimator for the digital adaptive gain controller.
class AdaptiveModeLevelEstimator {
 public:
  explicit AdaptiveModeLevelEstimator(ApmDataDumper* apm_data_dumper);
  AdaptiveModeLevelEstimator(const AdaptiveModeLevelEstimator&) = delete;
  AdaptiveModeLevelEstimator& operator=(const AdaptiveModeLevelEstimator&) =
      delete;
  // TODO(alessiob): Deprecate.
  AdaptiveModeLevelEstimator(
      ApmDataDumper* apm_data_dumper,
      AudioProcessing::Config::GainController2::LevelEstimator
          level_estimator_type,
      bool use_saturation_protector,
      float extra_saturation_margin_db);
  AdaptiveModeLevelEstimator(
      ApmDataDumper* apm_data_dumper,
      AudioProcessing::Config::GainController2::LevelEstimator
          level_estimator_type,
      int min_consecutive_speech_frames,
      bool use_saturation_protector,
      float initial_saturation_margin_db,
      float extra_saturation_margin_db);
  ~AdaptiveModeLevelEstimator();

  // Returns true if enough speech frames have been observed.
  bool IsConfident() const;
  // Returns the estimated speech plus noise level.
  float GetLevelDbfs() const;
  // Updates the level estimation.
  void Update(const VadWithLevel::LevelAndProbability& vad_data);

  void Reset();

 private:
  // Part of the level estimator state used for check-pointing and restore ops.
  struct State {
    bool operator==(const State& s) const;
    bool operator!=(const State& s) const;
    struct Ratio {
      float numerator;
      float denominator;
      float GetRatio() const;
    };
    int time_to_full_buffer_ms;
    absl::optional<Ratio> level_dbfs;
    SaturationProtectorState saturation_protector;
  };
  static_assert(std::is_trivially_copyable<State>::value, "");

  void ResetState(State& state);

  // Analyzes `vad_data` and `state` and updates `state`.
  void UpdateState(const VadWithLevel::LevelAndProbability& vad_data,
                   State& state);

  void DumpDebugData() const;

  ApmDataDumper* const apm_data_dumper_;
  const AudioProcessing::Config::GainController2::LevelEstimator
      level_estimator_type_;
  const int min_consecutive_speech_frames_;
  const bool use_saturation_protector_;
  const float saturation_protector_initial_margin_db_;
  const float saturation_protector_extra_margin_db_;
  State temporary_state_;
  State last_state_;
  absl::optional<float> last_level_dbfs_;
  int num_adjacent_speech_frames_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_MODE_LEVEL_ESTIMATOR_H_
