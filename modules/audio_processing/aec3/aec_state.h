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
#include "modules/audio_processing/aec3/delay_estimate.h"
#include "modules/audio_processing/aec3/echo_path_variability.h"
#include "modules/audio_processing/aec3/erl_estimator.h"
#include "modules/audio_processing/aec3/erle_estimator.h"
#include "modules/audio_processing/aec3/render_buffer.h"
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
  bool ActiveRender() const { return render_activity_.ActiveBlocks() > 200; }

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

  // Returns whether the echo in the capture signal is audible.
  bool InaudibleEcho() const { return echo_audibility_.InaudibleEcho(); }

  // Updates the aec state with the AEC output signal.
  void UpdateWithOutput(rtc::ArrayView<const float> e) {
    echo_audibility_.UpdateWithOutput(e);
  }

  // Returns whether the linear filter adaptation has recently started.
  bool StartupPhase() const { return startup_phase_; }

  // Returns wether a headset has been detected.
  bool TransparentMode() const { return headset_detector_.HeadsetDetected(); }

  // Returns the echo path gain.
  const rtc::Optional<float>& EchoPathGain() const {
    return filter_analyzer_.Gain();
  }

  // Returns whether the filter adaptation is still in the initial state.
  bool InitialState() const { return initial_state_; }

  // Returns whether the setup has clock_drift_.
  bool ClockDrift() const { return clock_drift_detector_.HasClockDrift(); }

  // Updates the aec state.
  void Update(const rtc::Optional<DelayEstimate>& delay_estimate,
              const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                  adaptive_filter_frequency_response,
              const std::vector<float>& adaptive_filter_impulse_response,
              bool converged_filter,
              const RenderBuffer& render_buffer,
              const std::array<float, kFftLengthBy2Plus1>& E2_main,
              const std::array<float, kFftLengthBy2Plus1>& Y2,
              const std::array<float, kBlockSize>& s_main);

 private:
  class EchoAudibility {
   public:
    void Update(rtc::ArrayView<const float> x,
                const std::array<float, kBlockSize>& s,
                bool converged_filter);
    void UpdateWithOutput(rtc::ArrayView<const float> e);
    bool InaudibleEcho() const { return inaudible_echo_; }

   private:
    float max_nearend_ = 0.f;
    size_t max_nearend_counter_ = 0;
    size_t low_farend_counter_ = 0;
    bool inaudible_echo_ = false;
  };

  class HeadsetDetector {
   public:
    HeadsetDetector();
    void Reset();
    void Update(const rtc::Optional<DelayEstimate>& delay_estimate,
                bool good_filter_estimate,
                bool converged_filter);
    bool HeadsetDetected() const { return headset_present_; }

   private:
    bool converged_filter_seen_ = false;
    float delay_update_measure_ = 0.f;
    float delay_change_measure_ = 0.f;
    bool headset_present_ = false;
    size_t old_delay_ = 0;
  };

  class EchoSaturationDetector {
   public:
    explicit EchoSaturationDetector(const EchoCanceller3Config& config);
    void Reset();
    void Update(rtc::ArrayView<const float> x_aligned,
                bool saturated_capture,
                const rtc::Optional<float>& echo_path_gain,
                bool good_filter_estimate);
    bool SaturationDetected() const { return echo_saturation_; }

   private:
    const bool can_saturate_;
    bool echo_saturation_ = false;
    size_t blocks_since_last_saturation_ = 0;
    float echo_path_gain_ = 160.f;
  };

  class EchoModelSelector {
   public:
    EchoModelSelector();
    void Reset();
    void Update(bool echo_saturation,
                bool converged_filter,
                size_t blocks_with_proper_filter_adaptation,
                size_t capture_blocks_counter);
    bool LinearModelSelected() const { return linear_model_selected_; }

   private:
    size_t blocks_since_converged_filter_ =
        std::numeric_limits<std::size_t>::max();
    bool linear_model_selected_ = false;
  };

  class RenderActivity {
   public:
    explicit RenderActivity(const EchoCanceller3Config& config);
    void Reset();
    void Update(rtc::ArrayView<const float> x_aligned, bool saturated_capture);
    size_t ActiveBlocks() const { return active_render_blocks_; }
    size_t ActiveBlocksWithoutSaturation() const {
      return active_render_blocks_with_no_saturation_;
    }

   private:
    const float active_render_limit_;
    size_t active_render_blocks_with_no_saturation_ = 0;
    size_t active_render_blocks_ = 0;
  };

  class FilterAnalyzer {
   public:
    explicit FilterAnalyzer(const EchoCanceller3Config& config);
    ~FilterAnalyzer();
    void Reset();
    void Update(rtc::ArrayView<const float> filter,
                bool converged_filter,
                bool clock_drift);
    int DelayBlocks() const { return delay_blocks_; }
    const rtc::Optional<float>& Gain() const { return gain_; }
    bool GoodEstimate() const { return good_estimate_; }

   private:
    const bool bounded_erl_;
    bool converged_filter_seen_ = false;
    size_t blocks_since_converged_filter_seen_ = 0;
    int delay_blocks_ = 0;
    rtc::Optional<float> gain_;
    size_t blocks_since_reset_ = 0;
    bool good_estimate_ = false;
  };

  class ClockDriftDetector {
   public:
    explicit ClockDriftDetector(const EchoCanceller3Config& config);
    void Reset();
    void Update(rtc::ArrayView<const float> filter,
                const rtc::Optional<DelayEstimate>& delay_estimate,
                bool converged_filter);
    int HasClockDrift() const { return clock_drift_; }

   private:
    const bool clock_drift_flagged_;
    bool clock_drift_ = false;
  };

  class SuppressionGainLimiter {
   public:
    explicit SuppressionGainLimiter(const EchoCanceller3Config& config);
    void Reset();
    void Update(bool render_activity);
    float Limit() const { return suppressor_gain_limit_; }

   private:
    const EchoCanceller3Config::EchoRemovalControl::GainRampup rampup_config_;
    const float gain_rampup_increase_;
    bool call_startup_phase_ = true;
    int realignment_counter_ = 0;
    bool active_render_seen_ = false;
    float suppressor_gain_limit_ = 1.f;
    bool recent_reset_ = false;
  };

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
  EchoAudibility echo_audibility_;
  HeadsetDetector headset_detector_;
  EchoModelSelector echo_model_selector_;
  EchoSaturationDetector echo_saturation_detector_;
  RenderActivity render_activity_;
  FilterAnalyzer filter_analyzer_;
  ClockDriftDetector clock_drift_detector_;
  SuppressionGainLimiter suppression_gain_limiter_;
  bool startup_phase_ = true;
  bool initial_state_ = true;

  RTC_DISALLOW_COPY_AND_ASSIGN(AecState);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_
