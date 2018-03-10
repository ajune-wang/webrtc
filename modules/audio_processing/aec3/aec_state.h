/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_
#define MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/optional.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/clock_drift_detector.h"
#include "modules/audio_processing/aec3/delay_estimate.h"
#include "modules/audio_processing/aec3/echo_audibility.h"
#include "modules/audio_processing/aec3/echo_model_selector.h"
#include "modules/audio_processing/aec3/echo_path_strength_detector.h"
#include "modules/audio_processing/aec3/echo_path_variability.h"
#include "modules/audio_processing/aec3/echo_saturation_detector.h"
#include "modules/audio_processing/aec3/erl_estimator.h"
#include "modules/audio_processing/aec3/erle_estimator.h"
#include "modules/audio_processing/aec3/filter_analyzer.h"
#include "modules/audio_processing/aec3/render_activity.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/suppression_gain_limiter.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;

// Handles the state and the conditions for the echo removal functionality.
class AecState {
 public:
  explicit AecState(const EchoCanceller3Config& config);
  ~AecState();

  // Returns whether the echo subtractor can be used to determine the residual
  // echo.
  bool LinearEchoModelFeasible() const {
    return echo_model_selector_.LinearModelSelected();
  }

  // Returns whether the render signal is currently active.
  bool ActiveRender() const { return render_activity_.NumActiveBlocks() > 200; }

  // Returns the ERLE.
  const std::array<float, kFftLengthBy2Plus1>& Erle() const {
    return erle_estimator_.Erle();
  }

  // Returns the time-domain ERLE.
  float ErleTimeDomain() const { return erle_estimator_.ErleTimeDomain(); }

  // Returns the ERL.
  const std::array<float, kFftLengthBy2Plus1>& Erl() const {
    return erl_estimator_.Erl();
  }

  // Returns the time-domain ERL.
  float ErlTimeDomain() const { return erl_estimator_.ErlTimeDomain(); }

  // Returns the delay estimate based on the linear filter.
  int FilterDelay() const { return filter_analyzer_.DelayBlocks(); }

  // Returns whether the capture signal is saturated.
  bool SaturatedCapture() const { return capture_signal_saturation_; }

  // Returns whether the echo signal is saturated.
  bool SaturatedEcho() const {
    return echo_saturation_detector_.SaturationDetected();
  }

  // Updates the capture signal saturation.
  void UpdateCaptureSaturation(bool capture_signal_saturation) {
    capture_signal_saturation_ = capture_signal_saturation;
  }

  // Takes appropriate action at an echo path change.
  void HandleEchoPathChange(const EchoPathVariability& echo_path_variability);

  // Returns the decay factor for the echo reverberation.
  float ReverbDecay() const { return reverb_decay_; }

  // Returns the upper limit for the echo suppression gain.
  float SuppressionGainLimit() const {
    return suppression_gain_limiter_.Limit();
  }

  // Returns whether the linear filter adaptation has recently started.
  bool StartupPhase() const { return startup_phase_; }

  // Returns wether a the estimated echo path strength.
  EchoPathStrengthDetector::Strength EchoPathStrength() const {
    return echo_path_strength_detector_.GetStrength();
  }

  // Returns the echo path gain.
  const rtc::Optional<float>& EchoPathGain() const {
    return filter_analyzer_.Gain();
  }

  // Returns whether the filter adaptation is still in the initial state.
  bool InitialState() const { return initial_state_; }

  // Returns whether the setup has clock_drift_.
  bool ClockDrift() const { return clock_drift_detector_.HasClockDrift(); }

  // Returns the appropriate scaling of the residual echo to match the
  // audibility.
  float ResidualEchoScaling() const {
    return echo_audibility_.ResidualEchoScaling();
  };

  // Returns the number of upcoming non-audible echo blocks.
  size_t NumNonAudibleBlocks() const {
    return echo_audibility_.NumNonAudibleBlocks();
  };

  // Updates the aec state.
  void Update(const rtc::Optional<DelayEstimate>& delay_estimate,
              const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                  adaptive_filter_frequency_response,
              const std::vector<float>& adaptive_filter_impulse_response,
              bool converged_filter,
              bool diverged_filter,
              const RenderBuffer& render_buffer,
              const std::array<float, kFftLengthBy2Plus1>& E2_main,
              const std::array<float, kFftLengthBy2Plus1>& Y2,
              const std::array<float, kBlockSize>& s);

 private:
  void UpdateReverb(const std::vector<float>& impulse_response);

  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const EchoCanceller3Config config_;
  ErlEstimator erl_estimator_;
  ErleEstimator erle_estimator_;
  size_t capture_block_counter_ = 0;
  bool capture_signal_saturation_ = false;
  bool recent_reset_ = true;
  float reverb_decay_to_test_ = 0.9f;
  float reverb_decay_candidate_ = 0.f;
  float reverb_decay_candidate_residual_ = -1.f;
  float reverb_decay_;
  EchoPathStrengthDetector echo_path_strength_detector_;
  EchoModelSelector echo_model_selector_;
  EchoSaturationDetector echo_saturation_detector_;
  RenderActivity render_activity_;
  FilterAnalyzer filter_analyzer_;
  ClockDriftDetector clock_drift_detector_;
  SuppressionGainUpperLimiter suppression_gain_limiter_;
  EchoAudibility echo_audibility_;
  bool startup_phase_ = true;
  bool initial_state_ = true;
  RTC_DISALLOW_COPY_AND_ASSIGN(AecState);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_
