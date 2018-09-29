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

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

float ComputeGainRampupIncrease(const EchoCanceller3Config& config) {
  const auto& c = config.echo_removal_control.gain_rampup;
  return powf(1.f / c.first_non_zero_gain, 1.f / c.non_zero_gain_blocks);
}

constexpr size_t kBlocksSinceConvergencedFilterInit = 10000;
constexpr size_t kBlocksSinceConsistentEstimateInit = 10000;

}  // namespace

int AecState::instance_count_ = 0;

void AecState::GetResidualEchoScaling(rtc::ArrayView<float> residual_scaling) const {
  bool filter_has_had_time_to_converge;
  if (config_.filter.conservative_initial_phase) {
    filter_has_had_time_to_converge =
        blocks_with_proper_filter_adaptation_ >= 1.5f * kNumBlocksPerSecond;
  } else {
    filter_has_had_time_to_converge =
        blocks_with_proper_filter_adaptation_ >= 0.8f * kNumBlocksPerSecond;
  }
  echo_audibility_.GetResidualEchoScaling(filter_has_had_time_to_converge,
                                          residual_scaling);
}

absl::optional<float> AecState::ErleUncertainty() const {
    bool filter_has_had_time_to_converge;
  if (config_.filter.conservative_initial_phase) {
    filter_has_had_time_to_converge =
        blocks_with_proper_filter_adaptation_ >= 1.5f * kNumBlocksPerSecond;
  } else {
    filter_has_had_time_to_converge =
        blocks_with_proper_filter_adaptation_ >= 0.8f * kNumBlocksPerSecond;
  }

    if (!filter_has_had_time_to_converge) {
      return 1.f;
    }
    return absl::nullopt;
  }



AecState::AecState(const EchoCanceller3Config& config)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      config_(config),
      initial_state_(config_),
      delay_state_(config_),
      transparent_state_(config_),
      filter_quality_state_(config_),
      saturation_detector_(config_),
      erle_estimator_(config_.erle.min, config_.erle.max_l, config_.erle.max_h),
      gain_rampup_increase_(ComputeGainRampupIncrease(config_)),
      suppression_gain_limiter_(config_),
      filter_analyzer_(config_),
      echo_audibility_(
          config_.echo_audibility.use_stationarity_properties_at_init),
      reverb_model_estimator_(config_) {}

AecState::~AecState() = default;

void AecState::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  const auto full_reset = [&]() {
    filter_analyzer_.Reset();
    capture_signal_saturation_ = false;
    blocks_with_proper_filter_adaptation_ = 0;
    render_received_ = false;
    blocks_with_active_render_ = 0;
    suppression_gain_limiter_.Reset();
    initial_state_.Reset();
    transparent_state_.Reset();
    saturation_detector_.Reset();
    erle_estimator_.Reset(true);
    erl_estimator_.Reset();
  };

  // TODO(peah): Refine the reset scheme according to the type of gain and
  // delay adjustment.

  if (echo_path_variability.delay_change !=
      EchoPathVariability::DelayAdjustment::kNone) {
    full_reset();
  }

  subtractor_output_analyzer_.HandleEchoPathChange();
}


void AecState::Update(
    const absl::optional<DelayEstimate>& external_delay,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        adaptive_filter_frequency_response,
    const std::vector<float>& adaptive_filter_impulse_response,
    const RenderBuffer& render_buffer,
    const std::array<float, kFftLengthBy2Plus1>& E2_main,
    const std::array<float, kFftLengthBy2Plus1>& Y2,
    const SubtractorOutput& subtractor_output,
    rtc::ArrayView<const float> y) {
  // Analyze the filter output.
  subtractor_output_analyzer_.Update(subtractor_output);

  // Analyze the filter and compute the delays.
  filter_analyzer_.Update(adaptive_filter_impulse_response,
                          adaptive_filter_frequency_response, render_buffer);

  delay_state_.Update(filter_analyzer_, external_delay,
                      blocks_with_proper_filter_adaptation_);

  const std::vector<float>& aligned_render_block =
      render_buffer.Block(-delay_state_.FilterDelayBlocks())[0];

  // Update counters.
  const bool active_render_block = DetectActiveRender(aligned_render_block);
  blocks_with_active_render_ += active_render_block ? 1 : 0;
  blocks_with_proper_filter_adaptation_ +=
      active_render_block && !SaturatedCapture() ? 1 : 0;

  // Update the limit on the echo suppr ession after an echo path change to avoid
  // an initial echo burst.
  suppression_gain_limiter_.Update(render_buffer.GetRenderActivity(),
                                   TransparentMode());
  if (subtractor_output_analyzer_.ConvergedFilter()) {
    suppression_gain_limiter_.Deactivate();
  }

  if (config_.echo_audibility.use_stationary_properties) {
    // Update the echo audibility evaluator.
    echo_audibility_.Update(
        render_buffer, FilterDelayBlocks(),
        delay_state_.ExternalDelayReported(),
        config_.ep_strength.reverb_based_on_render ? ReverbDecay() : 0.f);
  }

  // Update the ERL and ERLE measures.
  if (initial_state_.TransitionTriggered()) {
    erle_estimator_.Reset(false);
  }

  const auto& X2 = render_buffer.Spectrum(delay_state_.FilterDelayBlocks());
  erle_estimator_.Update(X2, Y2, E2_main,
                         subtractor_output_analyzer_.ConvergedFilter(),
                         config_.erle.onset_detection);

  erl_estimator_.Update(subtractor_output_analyzer_.ConvergedFilter(), X2, Y2);

  // Detect and flag echo saturation.
  saturation_detector_.Update(aligned_render_block, SaturatedCapture(), EchoPathGain());

  initial_state_.Update(active_render_block, SaturatedCapture());

  transparent_state_.Update(delay_state_.FilterDelayBlocks(),
                            filter_analyzer_.Consistent(),
                            subtractor_output_analyzer_.ConvergedFilter(),
                            subtractor_output_analyzer_.DivergedFilter(),
                            active_render_block, SaturatedCapture());

  filter_quality_state_.Update(saturation_detector_.SaturatedEcho(), active_render_block,
                               SaturatedCapture(), TransparentMode(),
                               external_delay,
                               subtractor_output_analyzer_.ConvergedFilter(),
                               subtractor_output_analyzer_.DivergedFilter());



  const bool stationary_block =
      config_.echo_audibility.use_stationary_properties &&
      echo_audibility_.IsBlockStationary();

  reverb_model_estimator_.Update(filter_analyzer_.GetAdjustedFilter(),
                                 adaptive_filter_frequency_response,
                                 erle_estimator_.GetInstLinearQualityEstimate(),
                                 delay_state_.FilterDelayBlocks(),
                                 UsableLinearEstimate(), stationary_block);

  erle_estimator_.Dump(data_dumper_);
  reverb_model_estimator_.Dump(data_dumper_.get());
  data_dumper_->DumpRaw("aec3_erl", Erl());
  data_dumper_->DumpRaw("aec3_erl_time_domain", ErlTimeDomain());
  data_dumper_->DumpRaw("aec3_usable_linear_estimate", UsableLinearEstimate());
  data_dumper_->DumpRaw("aec3_transparent_mode", TransparentMode());
  data_dumper_->DumpRaw("aec3_filter_delay", filter_analyzer_.DelayBlocks());

  data_dumper_->DumpRaw("aec3_consistent_filter",
                        filter_analyzer_.Consistent());
  data_dumper_->DumpRaw("aec3_suppression_gain_limit", SuppressionGainLimit());
  data_dumper_->DumpRaw("aec3_initial_state",
                        initial_state_.InitialStateActive());
  data_dumper_->DumpRaw("aec3_capture_saturation", SaturatedCapture());
  data_dumper_->DumpRaw("aec3_echo_saturation", saturation_detector_.SaturatedEcho());
  data_dumper_->DumpRaw("aec3_converged_filter",
                        subtractor_output_analyzer_.ConvergedFilter());
  data_dumper_->DumpRaw("aec3_diverged_filter",
                        subtractor_output_analyzer_.DivergedFilter());

  data_dumper_->DumpRaw("aec3_external_delay_avaliable",
                        external_delay ? 1 : 0);
  data_dumper_->DumpRaw("aec3_suppresion_gain_limiter_running",
                        IsSuppressionGainLimitActive());
  data_dumper_->DumpRaw("aec3_filter_tail_freq_resp_est",
                        GetReverbFrequencyResponse());
}

bool AecState::DetectActiveRender(rtc::ArrayView<const float> x) const {
  const float x_energy = std::inner_product(x.begin(), x.end(), x.begin(), 0.f);
  return x_energy > (config_.render_levels.active_render_limit *
                     config_.render_levels.active_render_limit) *
                        kFftLengthBy2;
}


AecState::InitialState::InitialState(const EchoCanceller3Config& config)
    : conservative_initial_phase_(config.filter.conservative_initial_phase),
      initial_state_seconds_(config.filter.initial_state_seconds) {
  Reset();
}
void AecState::InitialState::InitialState::Reset() {
  initial_state_ = true;
  blocks_with_proper_filter_adaptation_ = 0;
}
void AecState::InitialState::InitialState::Update(bool active_render_block,
                                                  bool saturated_capture) {
  blocks_with_proper_filter_adaptation_ +=
      active_render_block && !saturated_capture ? 1 : 0;


  // Flag whether the initial state is still active.
  bool prev_initial_state = initial_state_;
  if (conservative_initial_phase_) {
    initial_state_ =
        blocks_with_proper_filter_adaptation_ < 5 * kNumBlocksPerSecond;
  } else {
    initial_state_ = blocks_with_proper_filter_adaptation_ <
                     initial_state_seconds_ * kNumBlocksPerSecond;
  }
  transition_triggered_ = !initial_state_ && prev_initial_state;
}

AecState::DelayState::DelayState(const EchoCanceller3Config& config)
    : delay_headroom_blocks_(config.delay.delay_headroom_blocks) {}
void AecState::DelayState::Update(
    const FilterAnalyzer& filter_analyzer,
    const absl::optional<DelayEstimate>& external_delay,
    size_t blocks_with_proper_filter_adaptation) {
  filter_delay_blocks_ = filter_analyzer.DelayBlocks();
  if (external_delay &&
      (!external_delay_ || external_delay_->delay != external_delay->delay)) {
    frames_since_external_delay_change_ = 0;
    external_delay_ = external_delay;
  }
  if (blocks_with_proper_filter_adaptation < 2 * kNumBlocksPerSecond &&
      external_delay_) {
    filter_delay_blocks_ = delay_headroom_blocks_;
  }

  if (filter_analyzer.Consistent()) {
    internal_delay_ = filter_analyzer.DelayBlocks();
  } else {
    internal_delay_ = absl::nullopt;
  }

  external_delay_seen_ = external_delay_seen_ || external_delay;
}

AecState::TransparentState::TransparentState(const EchoCanceller3Config& config)
    : bounded_erl_(config.ep_strength.bounded_erl),
      linear_and_stable_echo_path_(
          config.echo_removal_control.linear_and_stable_echo_path),
      active_blocks_since_consistent_filter_estimate_(
          kBlocksSinceConsistentEstimateInit),
      blocks_since_converged_filter_(kBlocksSinceConvergencedFilterInit) {}
void AecState::TransparentState::Reset() {
  blocks_since_converged_filter_ = kBlocksSinceConvergencedFilterInit;
  diverged_blocks_ = 0;
  blocks_with_proper_filter_adaptation_ = 0;
  if (linear_and_stable_echo_path_) {
    converged_filter_seen_ = false;
  }
}

void AecState::TransparentState::Update(int filter_delay_blocks,
                                        bool consistent_filter,
                                        bool converged_filter,
                                        bool diverged_filter,
                                        bool active_render_block,
                                        bool saturated_capture) {
  ++capture_block_counter_;
  if (consistent_filter && filter_delay_blocks < 5) {
    consistent_filter_seen_ = true;
    active_blocks_since_consistent_filter_estimate_ = 0;
  } else if (active_render_block) {
    ++active_blocks_since_consistent_filter_estimate_;
  }

  if (converged_filter) {
    active_blocks_since_converged_filter_ = 0;
  } else if (active_render_block) {
    ++active_blocks_since_converged_filter_;
  }

  blocks_with_proper_filter_adaptation_ +=
      active_render_block && !saturated_capture ? 1 : 0;

  bool consistent_filter_estimate_not_seen;
  if (!consistent_filter_seen_) {
    consistent_filter_estimate_not_seen =
        capture_block_counter_ > 5 * kNumBlocksPerSecond;
  } else {
    consistent_filter_estimate_not_seen =
        active_blocks_since_consistent_filter_estimate_ >
        30 * kNumBlocksPerSecond;
  }

  // If no filter convergence is seen for a long time, reset the estimated
  // properties of the echo path.
  if (active_blocks_since_converged_filter_ > 60 * kNumBlocksPerSecond) {
    finite_erl_ = false;
  }

  diverged_blocks_ = diverged_filter ? diverged_blocks_ + 1 : 0;
  if (diverged_blocks_ >= 60) {
    blocks_since_converged_filter_ = kBlocksSinceConvergencedFilterInit;
  } else {
    blocks_since_converged_filter_ =
        converged_filter ? 0 : blocks_since_converged_filter_ + 1;
  }

  if (blocks_since_converged_filter_ > 20 * kNumBlocksPerSecond) {
    converged_filter_count_ = 0;
  } else if (converged_filter) {
    ++converged_filter_count_;
  }

  if (converged_filter_count_ > 50) {
    finite_erl_ = true;
  }

  converged_filter_seen_ = converged_filter_seen_ || converged_filter;

  // If no filter convergence is seen for a long time, reset the estimated
  // properties of the echo path.
  if (active_blocks_since_converged_filter_ > 60 * kNumBlocksPerSecond) {
    converged_filter_seen_ = false;
  }

  if (!filter_should_have_converged_) {
    filter_should_have_converged_ =
        blocks_with_proper_filter_adaptation_ > 6 * kNumBlocksPerSecond;
  }

  // After an amount of active render samples for which an echo should have been
  // detected in the capture signal if the ERL was not infinite, flag that a
  // transparent mode should be entered.
  transparent_mode_ = !bounded_erl_ && !finite_erl_;
  transparent_mode_ =
      transparent_mode_ &&
      (consistent_filter_estimate_not_seen || !converged_filter_seen_);
  transparent_mode_ = transparent_mode_ && filter_should_have_converged_;
}

AecState::FilterQualityState::FilterQualityState(
    const EchoCanceller3Config& config)
    : conservative_initial_phase_(config.filter.conservative_initial_phase),
      linear_and_stable_echo_path_(
          config.echo_removal_control.linear_and_stable_echo_path),
      blocks_since_converged_filter_(kBlocksSinceConvergencedFilterInit) {}

void AecState::FilterQualityState::Reset() {
  usable_linear_estimate_ = false;
  filter_has_had_time_to_converge_ = false;
  blocks_with_proper_filter_adaptation_ = 0;
  if (linear_and_stable_echo_path_) {
    converged_filter_seen_ = false;
  }
  blocks_since_converged_filter_ = kBlocksSinceConvergencedFilterInit;
  diverged_blocks_ = 0;
}
void AecState::FilterQualityState::Update(
    bool echo_saturation,
    bool active_render_block,
    bool saturated_capture,
    bool transparent_mode,
    const absl::optional<DelayEstimate>& external_delay,
    bool converged_filter,
    bool diverged_filter) {
  diverged_blocks_ = diverged_filter ? diverged_blocks_ + 1 : 0;
  if (diverged_blocks_ >= 60) {
    blocks_since_converged_filter_ = kBlocksSinceConvergencedFilterInit;
  } else {
    blocks_since_converged_filter_ =
        converged_filter ? 0 : blocks_since_converged_filter_ + 1;
  }

  if (converged_filter) {
    active_blocks_since_converged_filter_ = 0;
  } else if (active_render_block) {
    ++active_blocks_since_converged_filter_;
  }

  converged_filter_seen_ = converged_filter_seen_ || converged_filter;

  // If no filter convergence is seen for a long time, reset the estimated
  // properties of the echo path.
  if (active_blocks_since_converged_filter_ > 60 * kNumBlocksPerSecond) {
    converged_filter_seen_ = false;
  }

  blocks_with_proper_filter_adaptation_ +=
      active_render_block && !saturated_capture ? 1 : 0;

  if (conservative_initial_phase_) {
    filter_has_had_time_to_converge_ =
        blocks_with_proper_filter_adaptation_ >= 1.5f * kNumBlocksPerSecond;
  } else {
    filter_has_had_time_to_converge_ =
        blocks_with_proper_filter_adaptation_ >= 0.8f * kNumBlocksPerSecond;
  }

  usable_linear_estimate_ = !echo_saturation;

  if (conservative_initial_phase_) {
    usable_linear_estimate_ =
        usable_linear_estimate_ && filter_has_had_time_to_converge_;
  } else {
    usable_linear_estimate_ =
        usable_linear_estimate_ &&
        ((filter_has_had_time_to_converge_ && external_delay) ||
         converged_filter_seen_);
  }

  if (conservative_initial_phase_) {
    usable_linear_estimate_ = usable_linear_estimate_ && external_delay;
  }

  if (!linear_and_stable_echo_path_) {
    usable_linear_estimate_ =
        usable_linear_estimate_ &&
        blocks_since_converged_filter_ < 60 * kNumBlocksPerSecond;
  }
  usable_linear_estimate_ = usable_linear_estimate_ && !transparent_mode;

  use_linear_filter_output_ = usable_linear_estimate_ && !transparent_mode;
}

AecState::SaturationDetector::SaturationDetector(const EchoCanceller3Config& config) :echo_can_saturate_(config.ep_strength.echo_can_saturate), blocks_since_last_saturation_(1000){}
void AecState::SaturationDetector::Reset() {
  blocks_since_last_saturation_ = 0;
}

void AecState::SaturationDetector::Update(rtc::ArrayView<const float> x,
                                          bool saturated_capture,
                                          float echo_path_gain) {
  if (!echo_can_saturate_) {
    echo_saturation_ =  false;
    return;
  }

  RTC_DCHECK_LT(0, x.size());
  const float max_sample = fabs(*std::max_element(
      x.begin(), x.end(), [](float a, float b) { return a * a < b * b; }));

  // Set flag for potential presence of saturated echo
  const float kMargin = 10.f;
  float peak_echo_amplitude = max_sample * echo_path_gain * kMargin;
  if (saturated_capture && peak_echo_amplitude > 32000) {
    blocks_since_last_saturation_ = 0;
  } else {
    ++blocks_since_last_saturation_;
  }

  echo_saturation_ =  blocks_since_last_saturation_ < 5;
}

}  // namespace webrtc
