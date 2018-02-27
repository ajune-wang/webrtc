/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/aec_state.h"

#include <math.h>

#include <numeric>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

float ComputeGainRampupIncrease(
    const EchoCanceller3Config::EchoRemovalControl::GainRampup& rampup_config) {
  return powf(1.f / rampup_config.first_non_zero_gain,
              1.f / rampup_config.non_zero_gain_blocks);
}

}  // namespace

int AecState::instance_count_ = 0;

AecState::AecState(const EchoCanceller3Config& config)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      config_(config),
      erle_estimator_(config_.erle.min, config_.erle.max_l, config_.erle.max_h),
      reverb_decay_(config_.ep_strength.default_len),
      headset_detector_(),
      echo_model_selector_(),
      echo_saturation_detector_(config_),
      render_activity_(config_),
      filter_analyzer_(config_),
      clock_drift_detector_(config_),
      suppression_gain_limiter_(config_) {}

AecState::~AecState() = default;

void AecState::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  const auto full_reset = [&]() {
    headset_detector_.Reset();
    echo_model_selector_.Reset();
    echo_saturation_detector_.Reset();
    render_activity_.Reset();
    filter_analyzer_.Reset();
    clock_drift_detector_.Reset();
    suppression_gain_limiter_.Reset();
    capture_signal_saturation_ = false;
    capture_block_counter_ = 0;
    startup_phase_ = true;
    initial_state_ = true;
    recent_reset_ = true;
  };

  // TODO(peah): Refine the reset scheme according to the type of gain and
  // delay adjustment.
  if (echo_path_variability.gain_change) {
    full_reset();
  }

  if (echo_path_variability.delay_change !=
      EchoPathVariability::DelayAdjustment::kBufferReadjustment) {
    full_reset();
  } else if (echo_path_variability.delay_change !=
             EchoPathVariability::DelayAdjustment::kBufferFlush) {
    full_reset();
  } else if (echo_path_variability.delay_change !=
             EchoPathVariability::DelayAdjustment::kDelayReset) {
    full_reset();
  } else if (echo_path_variability.delay_change !=
             EchoPathVariability::DelayAdjustment::kNewDetectedDelay) {
    full_reset();
  } else if (echo_path_variability.gain_change) {
    capture_block_counter_ = kNumBlocksPerSecond;
  }
}

void AecState::Update(
    const rtc::Optional<DelayEstimate>& delay_estimate,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        adaptive_filter_frequency_response,
    const std::vector<float>& adaptive_filter_impulse_response,
    bool converged_filter,
    const RenderBuffer& render_buffer,
    const std::array<float, kFftLengthBy2Plus1>& E2_main,
    const std::array<float, kFftLengthBy2Plus1>& Y2,
    const std::array<float, kBlockSize>& s) {
  ++capture_block_counter_;

  // Detect clock drift.
  clock_drift_detector_.Update(adaptive_filter_impulse_response, delay_estimate,
                               converged_filter);

  // Analyze the filter and set the delay.
  filter_analyzer_.Update(adaptive_filter_impulse_response, converged_filter,
                          clock_drift_detector_.HasClockDrift());
  const std::vector<float>& x =
      render_buffer.Block(-filter_analyzer_.DelayBlocks())[0];

  // Track render activity.
  render_activity_.Update(x, SaturatedCapture());

  // Update the limit on the echo suppression after an echo path change to avoid
  // an initial echo burst.
  suppression_gain_limiter_.Update(render_buffer.GetRenderActivity());

  // Update the ERL and ERLE measures.
  const auto& X2 = render_buffer.Spectrum(filter_analyzer_.DelayBlocks());
  erle_estimator_.Update(X2, Y2, E2_main);
  erl_estimator_.Update(X2, Y2);

  // Update the echo audibility evaluator.
  echo_audibility_.Update(x, s, converged_filter);

  // Flag whether the echo removal is in its startup phase.
  startup_phase_ = render_activity_.ActiveBlocksWithoutSaturation() >=
                   1.5f * kNumBlocksPerSecond;

  // Determine whether the initial state is active.
  initial_state_ = render_activity_.ActiveBlocksWithoutSaturation() <
                   5 * kNumBlocksPerSecond;

  // Detect echo saturation.
  echo_saturation_detector_.Update(x, SaturatedCapture(),
                                   filter_analyzer_.Gain(),
                                   filter_analyzer_.GoodEstimate());

  // Estimate the quality of the linear filter.
  echo_model_selector_.Update(
      echo_saturation_detector_.SaturationDetected(), converged_filter,
      render_activity_.ActiveBlocksWithoutSaturation(), capture_block_counter_);

  // Detect whether a headset is present.
  headset_detector_.Update(delay_estimate, filter_analyzer_.GoodEstimate(),
                           converged_filter);

  data_dumper_->DumpRaw("aec3_erle", Erle());
  data_dumper_->DumpRaw("aec3_erl", Erl());
  data_dumper_->DumpRaw("aec3_erle_time_domain", ErleTimeDomain());
  data_dumper_->DumpRaw("aec3_erl_time_domain", ErlTimeDomain());
  data_dumper_->DumpRaw("aec3_linear_echo_model_selected",
                        LinearEchoModelFeasible());
  data_dumper_->DumpRaw("aec3_filter_delay", FilterDelay());
  data_dumper_->DumpRaw("aec3_filter_gain", filter_analyzer_.Gain()
                                                ? *filter_analyzer_.Gain()
                                                : -1.f);
  data_dumper_->DumpRaw("aec3_good_filter_estimate",
                        filter_analyzer_.GoodEstimate());
  data_dumper_->DumpRaw("aec3_suppression_gain_limit", SuppressionGainLimit());
  data_dumper_->DumpRaw("aec3_reverb_decay", ReverbDecay());
  data_dumper_->DumpRaw("aec3_capture_saturation", SaturatedCapture());
  data_dumper_->DumpRaw("aec3_initial_state", InitialState());
  data_dumper_->DumpRaw("aec3_inaudible_echo", InaudibleEcho());
  data_dumper_->DumpRaw("aec3_headset_detected",
                        headset_detector_.HeadsetDetected());
  data_dumper_->DumpRaw("aec3_startup_phase", StartupPhase());
  data_dumper_->DumpRaw("aec3_echo_saturation",
                        echo_saturation_detector_.SaturationDetected());
  data_dumper_->DumpRaw("aec3_reset", recent_reset_);
  data_dumper_->DumpRaw("aec3_converged_filter", converged_filter);

  recent_reset_ = false;
}

void AecState::UpdateReverb(const std::vector<float>& impulse_response) {
  if ((!(filter_analyzer_.DelayBlocks() &&
         echo_model_selector_.LinearModelSelected())) ||
      (filter_analyzer_.DelayBlocks() >
       static_cast<int>(config_.filter.main.length_blocks) - 4)) {
    return;
  }

  // Form the data to match against by squaring the impulse response
  // coefficients.
  std::array<float, GetTimeDomainLength(kMaxAdaptiveFilterLength)>
      matching_data_data;
  RTC_DCHECK_LE(GetTimeDomainLength(config_.filter.main.length_blocks),
                matching_data_data.size());
  rtc::ArrayView<float> matching_data(
      matching_data_data.data(),
      GetTimeDomainLength(config_.filter.main.length_blocks));
  std::transform(impulse_response.begin(), impulse_response.end(),
                 matching_data.begin(), [](float a) { return a * a; });

  // Avoid matching against noise in the model by subtracting an estimate of the
  // model noise power.
  constexpr size_t kTailLength = 64;
  const size_t tail_index =
      GetTimeDomainLength(config_.filter.main.length_blocks) - kTailLength;
  const float tail_power = *std::max_element(matching_data.begin() + tail_index,
                                             matching_data.end());
  std::for_each(matching_data.begin(), matching_data.begin() + tail_index,
                [tail_power](float& a) { a = std::max(0.f, a - tail_power); });

  // Identify the peak index of the impulse response.
  const size_t peak_index = *std::max_element(
      matching_data.begin(), matching_data.begin() + tail_index);

  if (peak_index + 128 < tail_index) {
    size_t start_index = peak_index + 64;
    // Compute the matching residual error for the current candidate to match.
    float residual_sqr_sum = 0.f;
    float d_k = reverb_decay_to_test_;
    for (size_t k = start_index; k < tail_index; ++k) {
      if (matching_data[start_index + 1] == 0.f) {
        break;
      }

      float residual = matching_data[k] - matching_data[peak_index] * d_k;
      residual_sqr_sum += residual * residual;
      d_k *= reverb_decay_to_test_;
    }

    // If needed, update the best candidate for the reverb decay.
    if (reverb_decay_candidate_residual_ < 0.f ||
        residual_sqr_sum < reverb_decay_candidate_residual_) {
      reverb_decay_candidate_residual_ = residual_sqr_sum;
      reverb_decay_candidate_ = reverb_decay_to_test_;
    }
  }

  // Compute the next reverb candidate to evaluate such that all candidates will
  // be evaluated within one second.
  reverb_decay_to_test_ += (0.9965f - 0.9f) / (5 * kNumBlocksPerSecond);

  // If all reverb candidates have been evaluated, choose the best one as the
  // reverb decay.
  if (reverb_decay_to_test_ >= 0.9965f) {
    if (reverb_decay_candidate_residual_ < 0.f) {
      // Transform the decay to be in the unit of blocks.
      reverb_decay_ = powf(reverb_decay_candidate_, kFftLengthBy2);

      // Limit the estimated reverb_decay_ to the maximum one needed in practice
      // to minimize the impact of incorrect estimates.
      reverb_decay_ = std::min(config_.ep_strength.default_len, reverb_decay_);
    }
    reverb_decay_to_test_ = 0.9f;
    reverb_decay_candidate_residual_ = -1.f;
  }

  // For noisy impulse responses, assume a fixed tail length.
  if (tail_power > 0.0005f) {
    reverb_decay_ = config_.ep_strength.default_len;
  }
  data_dumper_->DumpRaw("aec3_reverb_decay", reverb_decay_);
  data_dumper_->DumpRaw("aec3_tail_power", tail_power);
}

void AecState::EchoAudibility::Update(rtc::ArrayView<const float> x,
                                      const std::array<float, kBlockSize>& s,
                                      bool converged_filter) {
  auto result_x = std::minmax_element(x.begin(), x.end());
  auto result_s = std::minmax_element(s.begin(), s.end());
  const float x_abs =
      std::max(fabsf(*result_x.first), fabsf(*result_x.second));
  const float s_abs =
      std::max(fabsf(*result_s.first), fabsf(*result_s.second));

  if (converged_filter) {
    if (x_abs < 20.f) {
      ++low_farend_counter_;
    } else {
      low_farend_counter_ = 0;
    }
  } else {
    if (x_abs < 100.f) {
      ++low_farend_counter_;
    } else {
      low_farend_counter_ = 0;
    }
  }

  // The echo is deemed as not audible if the echo estimate is on the level of
  // the quantization noise in the FFTs and the nearend level is sufficiently
  // strong to mask that by ensuring that the playout and AGC gains do not boost
  // any residual echo that is below the quantization noise level. Furthermore,
  // cases where the render signal is very close to zero are also identified as
  // not producing audible echo.
  inaudible_echo_ = (max_nearend_ > 500 && s_abs < 30.f) ||
                    (!converged_filter && x_abs < 500);
  inaudible_echo_ = inaudible_echo_ || low_farend_counter_ > 20;
}

void AecState::EchoAudibility::UpdateWithOutput(rtc::ArrayView<const float> e) {
  const float e_max = *std::max_element(e.begin(), e.end());
  const float e_min = *std::min_element(e.begin(), e.end());
  const float e_abs = std::max(fabsf(e_max), fabsf(e_min));

  if (max_nearend_ < e_abs) {
    max_nearend_ = e_abs;
    max_nearend_counter_ = 0;
  } else {
    if (++max_nearend_counter_ > 5 * kNumBlocksPerSecond) {
      max_nearend_ *= 0.995f;
    }
  }
}

AecState::HeadsetDetector::HeadsetDetector() {
  Reset();
}

void AecState::HeadsetDetector::Reset() {
  converged_filter_seen_ = false;
  delay_update_measure_ = 0.f;
  delay_change_measure_ = 0.f;
  old_delay_ = 0;
}

void AecState::HeadsetDetector::Update(
    const rtc::Optional<DelayEstimate>& delay_estimate,
    bool good_filter_estimate,
    bool converged_filter) {
  constexpr float kForgettingFactor = 0.99f;
  if (delay_estimate) {
    delay_change_measure_ =
        kForgettingFactor * delay_change_measure_ +
        (delay_estimate->blocks_since_last_change == 0 ? 1 : 0);
    delay_update_measure_ =
        kForgettingFactor * delay_update_measure_ +
        (delay_estimate->blocks_since_last_update == 0 ? 1 : 0);
  }

  bool stable_delay_estimate =
      delay_estimate &&
      (delay_estimate->delay == old_delay_ &&
       delay_change_measure_ < 0.001f * delay_update_measure_ &&
       delay_estimate->blocks_since_last_change > 4 * kNumBlocksPerSecond);
  converged_filter_seen_ = converged_filter_seen_ || converged_filter;

  headset_present_ = !converged_filter_seen_ && !stable_delay_estimate &&
                     !good_filter_estimate;
  if (delay_estimate) {
    old_delay_ = delay_estimate->delay;
  }
}

AecState::EchoSaturationDetector::EchoSaturationDetector(
    const EchoCanceller3Config& config)
    : can_saturate_(config.ep_strength.echo_can_saturate) {
  Reset();
}

void AecState::EchoSaturationDetector::Reset() {
  echo_saturation_ = false;
  blocks_since_last_saturation_ = std::numeric_limits<std::size_t>::max();
  ;
  echo_path_gain_ = 160;
}

void AecState::EchoSaturationDetector::Update(
    rtc::ArrayView<const float> x_aligned,
    bool saturated_capture,
    const rtc::Optional<float>& echo_path_gain,
    bool good_filter_estimate) {
  if (!can_saturate_) {
    echo_saturation_ = false;
    return;
  }

  RTC_DCHECK_LT(0, x_aligned.size());
  const float x_max =
      fabs(*std::max_element(x_aligned.begin(), x_aligned.end(),
                             [](float a, float b) { return a * a < b * b; }));

  if (good_filter_estimate && echo_path_gain) {
    echo_path_gain_ = *echo_path_gain;
  }

  constexpr float kMargin = 10.f;
  bool potentially_saturating_echo = kMargin * echo_path_gain_ * x_max > 32000;

  blocks_since_last_saturation_ =
      potentially_saturating_echo && saturated_capture
          ? 0
          : blocks_since_last_saturation_ + 1;

  echo_saturation_ = blocks_since_last_saturation_ < 20;
}

AecState::EchoModelSelector::EchoModelSelector() {
  Reset();
}

void AecState::EchoModelSelector::Reset() {
  blocks_since_converged_filter_ = std::numeric_limits<std::size_t>::max();
  linear_model_selected_ = false;
}

void AecState::EchoModelSelector::Update(
    bool echo_saturation,
    bool converged_filter,
    size_t blocks_with_proper_filter_adaptation,
    size_t capture_blocks_counter) {
  bool filter_has_had_time_to_converge =
      blocks_with_proper_filter_adaptation >= 1.5f * kNumBlocksPerSecond;
  blocks_since_converged_filter_ =
      converged_filter ? 0 : blocks_since_converged_filter_ + 1;

  linear_model_selected_ =
      !echo_saturation &&
      (blocks_since_converged_filter_ < 2 * kNumBlocksPerSecond &&
       filter_has_had_time_to_converge) &&
      capture_blocks_counter >= 1.f * kNumBlocksPerSecond;
}

AecState::RenderActivity::RenderActivity(const EchoCanceller3Config& config)
    : active_render_limit_(config.render_levels.active_render_limit *
                           config.render_levels.active_render_limit *
                           kFftLengthBy2) {
  Reset();
}

void AecState::RenderActivity::Reset() {
  active_render_blocks_with_no_saturation_ = 0;
  active_render_blocks_ = 0;
}

void AecState::RenderActivity::Update(rtc::ArrayView<const float> x_aligned,
                                      bool saturated_capture) {
  bool activity =
      std::inner_product(x_aligned.begin(), x_aligned.end(), x_aligned.begin(),
                         0.f) > active_render_limit_;
  active_render_blocks_with_no_saturation_ +=
      activity && !saturated_capture ? 1 : 0;
  active_render_blocks_ += activity ? 1 : 0;
}

AecState::FilterAnalyzer::FilterAnalyzer(const EchoCanceller3Config& config)
    : bounded_erl_(config.ep_strength.bounded_erl) {
  Reset();
}

AecState::FilterAnalyzer::~FilterAnalyzer() = default;

void AecState::FilterAnalyzer::Reset() {
  delay_blocks_ = 0;
  good_estimate_ = false;
  blocks_since_reset_ = 0;
  converged_filter_seen_ = false;
}

void AecState::FilterAnalyzer::Update(rtc::ArrayView<const float> filter,
                                      bool converged_filter,
                                      bool clock_drift) {
  blocks_since_converged_filter_seen_ =
      converged_filter ? 0 : blocks_since_converged_filter_seen_ + 1;
  converged_filter_seen_ = converged_filter_seen_ || converged_filter;
  if (blocks_since_converged_filter_seen_ > 10 * kNumBlocksPerSecond) {
    converged_filter_seen_ = false;
  }

  size_t max_index = 0;
  float max_h2 = filter[0] * filter[0];
  for (size_t k = 1; k < filter.size(); ++k) {
    float tmp = filter[k] * filter[k];
    if (tmp > max_h2) {
      max_index = k;
      max_h2 = tmp;
    }
  }

  constexpr size_t kN1Size = 20;
  constexpr size_t kN2Size = 64;

  const auto sum_of_squares = [](float a, float b) { return a + b * b; };
  size_t n1_size = max_index > 1 ? std::min(kN1Size, max_index - 1) : 0;
  RTC_DCHECK_GE(filter.size(), n1_size);
  float n1 = std::accumulate(filter.begin(), filter.begin() + n1_size, 0.f,
                             sum_of_squares);
  size_t n2_size = std::min(kN2Size, filter.size() - max_index);
  RTC_DCHECK_GE(filter.size(), n2_size);
  float n2 = std::accumulate(filter.end() - n2_size, filter.end(), 0.f,
                             sum_of_squares);

  constexpr float kOneByN1Size = 1.f / kN1Size;
  constexpr float kOneByN2Size = 1.f / kN2Size;
  n1 = n1_size == kN1Size ? n1 * kOneByN1Size : n1 / n1_size;
  n2 = n2_size == kN2Size ? n2 * kOneByN2Size : n2 / n2_size;

  good_estimate_ = converged_filter_seen_ && max_h2 > 10 * std::max(n1, n2);
  delay_blocks_ = good_estimate_ ? max_index >> kBlockSizeLog2 : 0;

  if (++blocks_since_reset_ > 5 * kNumBlocksPerSecond && good_estimate_) {
    gain_ = fabs(filter[max_index]);
  } else {
    if (gain_) {
      gain_ = std::max(*gain_, fabs(filter[max_index]));
    }
  }

  if (bounded_erl_ && gain_) {
    *gain_ = std::max(*gain_, 0.01f);
  }
}

AecState::ClockDriftDetector::ClockDriftDetector(
    const EchoCanceller3Config& config)
    : clock_drift_flagged_(false) {
  Reset();
}

void AecState::ClockDriftDetector::Reset() {
  clock_drift_ = false;
}

void AecState::ClockDriftDetector::Update(
    rtc::ArrayView<const float> filter,
    const rtc::Optional<DelayEstimate>& delay_estimate,
    bool converged_filter) {
  clock_drift_ = clock_drift_flagged_;
}

AecState::SuppressionGainLimiter::SuppressionGainLimiter(
    const EchoCanceller3Config& config)
    : rampup_config_(config.echo_removal_control.gain_rampup),
      gain_rampup_increase_(ComputeGainRampupIncrease(rampup_config_)) {
  Reset();
}

void AecState::SuppressionGainLimiter::Reset() {
  recent_reset_ = true;
}

void AecState::SuppressionGainLimiter::Update(bool render_activity) {
  if (recent_reset_ && !call_startup_phase_) {
    constexpr int kMuteFramesAfterReset = kNumBlocksPerSecond / 4;
    realignment_counter_ = kMuteFramesAfterReset;
  } else if (!active_render_seen_ && render_activity) {
    active_render_seen_ = true;
    realignment_counter_ = rampup_config_.full_gain_blocks;
  } else if (realignment_counter_ > 0) {
    if (realignment_counter_ > 0) {
      --realignment_counter_;
      if (realignment_counter_ == 0) {
        call_startup_phase_ = false;
      }
    }
  }
  recent_reset_ = false;

  if (realignment_counter_ <= 0) {
    suppressor_gain_limit_ = 1.f;
    return;
  }

  if (realignment_counter_ > rampup_config_.non_zero_gain_blocks ||
      (!call_startup_phase_ && realignment_counter_ > 0)) {
    suppressor_gain_limit_ = 0.f;
    return;
  }

  if (realignment_counter_ == rampup_config_.non_zero_gain_blocks) {
    suppressor_gain_limit_ = rampup_config_.first_non_zero_gain;
    return;
  }

  RTC_DCHECK_LT(0.f, suppressor_gain_limit_);
  suppressor_gain_limit_ =
      std::min(1.f, suppressor_gain_limit_ * gain_rampup_increase_);
  RTC_DCHECK_GE(1.f, suppressor_gain_limit_);
}

}  // namespace webrtc
