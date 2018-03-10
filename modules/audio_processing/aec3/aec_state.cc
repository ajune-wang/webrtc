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

int AecState::instance_count_ = 0;

AecState::AecState(const EchoCanceller3Config& config)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      config_(config),
      erle_estimator_(config_.erle.min, config_.erle.max_l, config_.erle.max_h),
      reverb_decay_(config_.ep_strength.default_len),
      echo_path_strength_detector_(),
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
    echo_path_strength_detector_.Reset();
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
    bool diverged_filter,
    const RenderBuffer& render_buffer,
    const std::array<float, kFftLengthBy2Plus1>& E2_main,
    const std::array<float, kFftLengthBy2Plus1>& Y2,
    const std::array<float, kBlockSize>& s) {
  ++capture_block_counter_;

  // Detect clock drift.
  clock_drift_detector_.Update(adaptive_filter_impulse_response, delay_estimate,
                               converged_filter);

  // Analyze the filter and set the delay.
  filter_analyzer_.Update(adaptive_filter_impulse_response,
                          adaptive_filter_frequency_response, converged_filter,
                          clock_drift_detector_.HasClockDrift());
  const std::vector<float>& x =
      render_buffer.Block(-filter_analyzer_.DelayBlocks())[0];

  // Update the echo audibility evaluator.
  echo_audibility_.Update(render_buffer, FilterDelay(), s);

  // Track render activity.
  render_activity_.Update(x, SaturatedCapture());

  // Update the limit on the echo suppression after an echo path change to avoid
  // an initial echo burst.
  suppression_gain_limiter_.Update(render_buffer.GetRenderActivity());

  // Update the ERL and ERLE measures.
  const auto& X2 = render_buffer.Spectrum(filter_analyzer_.DelayBlocks());
  erle_estimator_.Update(X2, Y2, E2_main);
  erl_estimator_.Update(X2, Y2);

  // Flag whether the echo removal is in its startup phase.
  startup_phase_ = render_activity_.NumActiveBlocksWithoutSaturation() >=
                   1.5f * kNumBlocksPerSecond;

  // Determine whether the initial state is active.
  initial_state_ = render_activity_.NumActiveBlocksWithoutSaturation() <
                   5 * kNumBlocksPerSecond;

  // Detect echo saturation.
  echo_saturation_detector_.Update(x, SaturatedCapture(),
                                   filter_analyzer_.Gain(),
                                   filter_analyzer_.GoodEstimate());

  // Estimate the quality of the linear filter.
  echo_model_selector_.Update(
      echo_saturation_detector_.SaturationDetected(), converged_filter,
      diverged_filter, render_activity_.NumActiveBlocksWithoutSaturation(),
      capture_block_counter_);

  // Detect whether a headset is present.
  echo_path_strength_detector_.Update(
      delay_estimate, render_activity_.ActiveBlock(),
      filter_analyzer_.GoodEstimate(), converged_filter);

  data_dumper_->DumpRaw("aec3_erle", Erle());
  data_dumper_->DumpRaw("aec3_erl", Erl());
  data_dumper_->DumpRaw("aec3_erle_time_domain", ErleTimeDomain());
  data_dumper_->DumpRaw("aec3_erl_time_domain", ErlTimeDomain());
  data_dumper_->DumpRaw("aec3_linear_echo_model_selected",
                        LinearEchoModelFeasible());
  data_dumper_->DumpRaw("aec3_filter_delay", FilterDelay());
  data_dumper_->DumpRaw("aec3_filter_gain", filter_analyzer_.Gain()
                                                ? *filter_analyzer_.Gain()
                                                : 0.f);
  data_dumper_->DumpRaw("aec3_good_filter_estimate",
                        filter_analyzer_.GoodEstimate());
  data_dumper_->DumpRaw("aec3_suppression_gain_limit",
                        SuppressionGainLimit());
  data_dumper_->DumpRaw("aec3_reverb_decay", ReverbDecay());
  data_dumper_->DumpRaw("aec3_capture_saturation", SaturatedCapture());
  data_dumper_->DumpRaw("aec3_initial_state", InitialState());
  data_dumper_->DumpRaw(
      "aec3_echo_path_strength",
      static_cast<int16_t>(echo_path_strength_detector_.GetStrength()));
  data_dumper_->DumpRaw("aec3_startup_phase", StartupPhase());
  data_dumper_->DumpRaw("aec3_echo_saturation",
                        echo_saturation_detector_.SaturationDetected());
  data_dumper_->DumpRaw("aec3_reset", recent_reset_);
  data_dumper_->DumpRaw("aec3_converged_filter", converged_filter);
  data_dumper_->DumpRaw("aec3_diverged_filter", diverged_filter);

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

}  // namespace webrtc
