/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/filter_settings_generator.h"

#include <algorithm>
#include <cmath>

#include "rtc_base/checks.h"

namespace webrtc {

FilterSettingsGenerator::FilterSettingsGenerator(
    const RationalFunctionParameters& function_params,
    const ErrorThresholds& default_error_thresholds,
    const TransientParameters& transient_params)
    : function_params_(function_params),
      error_thresholds_(default_error_thresholds),
      transient_params_(transient_params),
      frames_since_keyframe_(0) {
  // TODO: Validate parameters
}

FilterSettingsGenerator::FilterSettingsGenerator(
    const ExponentialFunctionParameters& function_params,
    const ErrorThresholds& default_error_thresholds,
    const TransientParameters& transient_params)
    : function_params_(function_params),
      error_thresholds_(default_error_thresholds),
      transient_params_(transient_params),
      frames_since_keyframe_(0) {
  // TODO: Validate parameters
}

CorruptionDetectionFilterSettings FilterSettingsGenerator::OnFrame(
    bool is_keyframe,
    int qp) {
  double std_dev = CalculateStdDev(qp);
  int y_err = error_thresholds_.luma;
  int uv_err = error_thresholds_.chroma;

  if (is_keyframe || (transient_params_.large_qp_change_threshold > 0 &&
                      std::abs(previoius_qp_.value_or(qp) - qp) >=
                          transient_params_.large_qp_change_threshold)) {
    frames_since_keyframe_ = 0;
  }

  if (frames_since_keyframe_ <=
      transient_params_.keyframe_offset_duration_frames) {
    // The progress, from the start at the keyframe at 0.0 to completely back to
    // normal at 1.0.
    double progress = frames_since_keyframe_ /
                      (transient_params_.keyframe_offset_duration_frames + 1);
    double adjusted_std_dev =
        std::min(std_dev + transient_params_.keyframe_stddev_offset, 40.0);
    double adjusted_y_err =
        std::min(y_err + transient_params_.keyframe_threshold_offset, 15);
    double adjusted_uv_err =
        std::min(uv_err + transient_params_.keyframe_threshold_offset, 15);

    std_dev = ((1.0 - progress) * adjusted_std_dev) + (progress * std_dev);
    y_err = static_cast<int>(((1.0 - progress) * adjusted_y_err) +
                             (progress * y_err) + 0.5);
    uv_err = static_cast<int>(((1.0 - progress) * adjusted_uv_err) +
                              (progress * uv_err) + 0.5);
  }

  return CorruptionDetectionFilterSettings{.std_dev = std_dev,
                                           .luma_error_threshold = y_err,
                                           .chroma_error_threshold = uv_err};
}

double FilterSettingsGenerator::CalculateStdDev(int qp) const {
  if (absl::holds_alternative<RationalFunctionParameters>(function_params_)) {
    const auto& params =
        absl::get<RationalFunctionParameters>(function_params_);
    return (qp * params.numerator_factor) / (qp + params.denumerator_term) +
           params.offset;
  }
  RTC_DCHECK(
      absl::holds_alternative<ExponentialFunctionParameters>(function_params_));

  const auto& params =
      absl::get<ExponentialFunctionParameters>(function_params_);
  return params.scale *
         std::exp(params.exponent_factor * qp - params.exponent_offset);
}

}  // namespace webrtc
