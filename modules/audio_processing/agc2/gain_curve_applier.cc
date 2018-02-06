/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/gain_curve_applier.h"

#include <algorithm>

#include "api/array_view.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// This constant affects the way scaling factors are interpolated for the first
// sub-frame of a frame. Only in the case in which the first sub-frame has an
// estimated level which is greater than the that of the previous analyzed
// sub-frame, linear interpolation is replaced with a power function which
// reduces the chances of over-shooting (and hence saturation), however reducing
// the fixed gain effectiveness.
constexpr float kAttackFirstSubframeInterpolationPower = 8.f;

void InterpolateFirstSubframe(float last_factor,
                              float current_factor,
                              rtc::ArrayView<float> subframe) {
  const auto n = subframe.size();
  const auto p = kAttackFirstSubframeInterpolationPower;
  for (size_t i = 0; i < n; ++i) {
    subframe[i] = std::pow(1.f - i / n, p) * (last_factor - current_factor) +
                  current_factor;
  }
}

void ScaleSamples(rtc::ArrayView<const float> per_sample_scaling_factors,
                  MutableFloatAudioFrame signal) {
  const size_t samples_per_channel = signal.samples_per_channel();
  RTC_DCHECK_EQ(samples_per_channel, per_sample_scaling_factors.size());
  for (size_t i = 0; i < signal.num_channels(); ++i) {
    auto channel = signal.channel(i);
    for (size_t j = 0; j < samples_per_channel; ++j) {
      channel[j] *= per_sample_scaling_factors[j];
    }
  }
}

void ComputePerSampleSubframeFactors(
    const std::array<float, kSubFramesInFrame + 1>& scaling_factors,
    int samples_per_channel,
    rtc::ArrayView<float> per_sample_scaling_factors) {
  const int num_subframes = scaling_factors.size() - 1;
  const size_t subframe_size =
      rtc::CheckedDivExact(samples_per_channel, num_subframes);

  // Handle first sub-frame differently in case of attack.
  const bool is_attack = scaling_factors[0] > scaling_factors[1];
  if (is_attack) {
    InterpolateFirstSubframe(
        scaling_factors[0], scaling_factors[1],
        rtc::ArrayView<float>(
            per_sample_scaling_factors.subview(0, subframe_size)));
  }

  for (int i = is_attack ? 1 : 0; i < num_subframes; ++i) {
    const size_t subframe_start = i * subframe_size;
    const float scaling_start = scaling_factors[i];
    const float scaling_end = scaling_factors[i + 1];
    const float scaling_diff = (scaling_end - scaling_start) / subframe_size;
    for (size_t j = 0; j < subframe_size; ++j) {
      per_sample_scaling_factors[subframe_start + j] =
          scaling_start + scaling_diff * j;
    }
  }
}

}  // namespace

GainCurveApplier::GainCurveApplier(
    const InterpolatedGainCurve* interp_gain_curve,
    agc2::LevelEstimator* estimator,
    ApmDataDumper* apm_data_dumper)
    : interp_gain_curve_(*interp_gain_curve),
      level_estimator_(*estimator),
      apm_data_dumper_(apm_data_dumper) {}

GainCurveApplier::GainCurveApplier(GainCurveApplier&&) = default;

GainCurveApplier::~GainCurveApplier() = default;

void GainCurveApplier::Process(MutableFloatAudioFrame signal) {
  const auto level_estimate = level_estimator_.ComputeLevel(signal);

  RTC_DCHECK_EQ(level_estimate.size() + 1, scaling_factors_.size());
  scaling_factors_[0] = last_scaling_factor_;
  std::transform(level_estimate.begin(), level_estimate.end(),
                 scaling_factors_.begin() + 1, [this](float x) {
                   return interp_gain_curve_.LookUpGainToApply(x);
                 });

  const size_t samples_per_channel = signal.samples_per_channel();
  RTC_DCHECK_LE(samples_per_channel, kMaximalNumberOfSamplesPerChannel);

  auto per_sample_scaling_factors = rtc::ArrayView<float>(
      &per_sample_scaling_factors_[0], samples_per_channel);
  ComputePerSampleSubframeFactors(scaling_factors_, samples_per_channel,
                                  per_sample_scaling_factors);
  ScaleSamples(per_sample_scaling_factors, signal);

  last_scaling_factor_ = scaling_factors_.back();

  // Dump data for debug.
  apm_data_dumper_->DumpRaw("agc2_gain_curve_applier_scaling_factors",
                            per_sample_scaling_factors_.size(),
                            per_sample_scaling_factors_.data());
}

InterpolatedGainCurve::Stats GainCurveApplier::GetGainCurveStats() const {
  return interp_gain_curve_.get_stats();
}

}  // namespace webrtc
