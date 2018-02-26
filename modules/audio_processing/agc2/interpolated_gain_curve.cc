/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/interpolated_gain_curve.h"

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

constexpr std::array<float, kInterpolatedGainCurveTotalPoints>
    InterpolatedGainCurve::approximation_params_x_;

constexpr std::array<float, kInterpolatedGainCurveTotalPoints>
    InterpolatedGainCurve::approximation_params_m_;

constexpr std::array<float, kInterpolatedGainCurveTotalPoints>
    InterpolatedGainCurve::approximation_params_q_;

InterpolatedGainCurve::InterpolatedGainCurve(ApmDataDumper* apm_data_dumper)
    : apm_data_dumper_(apm_data_dumper) {}

InterpolatedGainCurve::~InterpolatedGainCurve() {
  if (stats_.available) {
    // TODO(alessiob): We might want to add these stats as RTC metrics.
    RTC_DCHECK(apm_data_dumper_);
    apm_data_dumper_->DumpRaw("agc2_interp_gain_curve_lookups_identity",
                              stats_.look_ups_identity_region);
    apm_data_dumper_->DumpRaw("agc2_interp_gain_curve_lookups_knee",
                              stats_.look_ups_knee_region);
    apm_data_dumper_->DumpRaw("agc2_interp_gain_curve_lookups_limiter",
                              stats_.look_ups_limiter_region);
    apm_data_dumper_->DumpRaw("agc2_interp_gain_curve_lookups_saturation",
                              stats_.look_ups_saturation_region);
  }
}

void InterpolatedGainCurve::UpdateStats(float input_level) const {
  stats_.available = true;

  enum GainCurveRegion {
    kIdentity = 0,
    kKnee = 1,
    kLimiter = 2,
    kSaturation = 3
  };

  GainCurveRegion region;

  if (input_level < approximation_params_x_[0]) {
    stats_.look_ups_identity_region++;
    region = GainCurveRegion::kIdentity;
  } else if (input_level <
             approximation_params_x_[kInterpolatedGainCurveKneePoints - 1]) {
    stats_.look_ups_knee_region++;
    region = GainCurveRegion::kKnee;
  } else if (input_level < kMaxInputLevelLinear) {
    stats_.look_ups_limiter_region++;
    region = GainCurveRegion::kLimiter;
  } else {
    stats_.look_ups_saturation_region++;
    region = GainCurveRegion::kSaturation;
  }

  // Log current region every 2000 * kFrameDurationMs / kSubFramesInFrame
  // seconds. For kFrameDurationMs=10, kSubFramesInFrame=20, this happens every
  // second.
  uma_logging_counter_++;
  if (uma_logging_counter_ > 2000) {
    uma_logging_counter_ = 0;
    RTC_HISTOGRAM_ENUMERATION("WebRTC.Audio.AGC2.FixedDigitalGainCurveRegion",
                              region, kSaturation);
  }
}

// Looks up a gain to apply given a non-negative input level.
// The cost of this operation depends on the region in which |input_level|
// falls.
// For the identity and the saturation regions the cost is O(1).
// For the other regions, namely knee and limiter, the cost is
// O(2 + log2(|LightkInterpolatedGainCurveTotalPoints|), plus O(1) for the
// linear interpolation (one product and one sum).
float InterpolatedGainCurve::LookUpGainToApply(float input_level) const {
  UpdateStats(input_level);

  if (input_level <= approximation_params_x_[0]) {
    // Identity region.
    return 1.0f;
  }

  if (input_level >= kMaxInputLevelLinear) {
    // Saturating lower bound. The saturing samples exactly hit the clipping
    // level. This method achieves has the lowest harmonic distorsion, but it
    // may reduce the amplitude of the non-saturating samples too much.
    return 32768.f / input_level;
  }

  // Knee and limiter regions; find the linear piece index. Spelling
  // out the complete type was the only way to silence both the clang
  // plugin and the windows compilers.
  std::array<float, kInterpolatedGainCurveTotalPoints>::const_iterator it =
      std::lower_bound(approximation_params_x_.begin(),
                       approximation_params_x_.end(), input_level);
  const size_t index = std::distance(approximation_params_x_.begin(), it) - 1;
  RTC_DCHECK_LE(0, index);
  RTC_DCHECK_LT(index, approximation_params_m_.size());
  RTC_DCHECK_LE(approximation_params_x_[index], input_level);
  if (index < approximation_params_m_.size() - 1) {
    RTC_DCHECK_LE(input_level, approximation_params_x_[index + 1]);
  }

  // Piece-wise linear interploation.
  const float gain = approximation_params_m_[index] * input_level +
                     approximation_params_q_[index];
  RTC_DCHECK_LE(0.f, gain);
  return gain;
}

}  // namespace webrtc
