/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/encoder_overshoot_detector.h"

#include <algorithm>

namespace webrtc {

EncoderOvershootDetector::EncoderOvershootDetector(int64_t window_size_ms)
    : window_size_ms_(window_size_ms),
      time_last_update_ms_(-1),
      sum_utilization_factors_(0.0),
      target_bitrate_(DataRate::Zero()),
      target_framerate_fps_(0),
      buffer_level_bits_(0) {}

EncoderOvershootDetector::~EncoderOvershootDetector() = default;

void EncoderOvershootDetector::SetTargetRate(DataRate target_bitrate,
                                             int target_framerate_fps,
                                             int64_t time_ms) {
  if (target_bitrate_ == DataRate::Zero() &&
      target_bitrate != DataRate::Zero()) {
    // Stream was just enabled, reset state.
    time_last_update_ms_ = time_ms;
    utilization_factors_.clear();
    sum_utilization_factors_ = 0;
    buffer_level_bits_ = 0;
  }

  target_bitrate_ = target_bitrate;
  target_framerate_fps_ = target_framerate_fps;
}

void EncoderOvershootDetector::OnEncodedFrame(size_t bytes, int64_t time_ms) {
  LeakBits(time_ms);
  const int64_t ideal_frame_size = IdealFrameSizeBits();
  const int64_t bitsum = (bytes * 8) + buffer_level_bits_;

  int64_t overshoot_bits = 0;
  if (bitsum > ideal_frame_size) {
    overshoot_bits = std::min(buffer_level_bits_, bitsum - ideal_frame_size);
  }

  double frame_utilization_factor =
      1.0 + (static_cast<double>(overshoot_bits) / ideal_frame_size);
  utilization_factors_.emplace_back(frame_utilization_factor, time_ms);
  sum_utilization_factors_ += frame_utilization_factor;

  buffer_level_bits_ -= overshoot_bits;
  buffer_level_bits_ += bytes * 8;
}

absl::optional<double> EncoderOvershootDetector::GetUtilizationFactor(
    int64_t time_ms) {
  // Cull old data points.
  while (!utilization_factors_.empty() &&
         (time_ms - utilization_factors_.front().update_time_ms) >
             window_size_ms_) {
    sum_utilization_factors_ -= utilization_factors_.front().utilization_factor;
    RTC_DCHECK_GE(sum_utilization_factors_, 0.0);
    utilization_factors_.pop_front();
  }

  // No data points within window, return.
  if (utilization_factors_.empty()) {
    return absl::nullopt;
  }

  return sum_utilization_factors_ / utilization_factors_.size();
}

void EncoderOvershootDetector::Reset() {
  time_last_update_ms_ = -1;
  utilization_factors_.clear();
  target_bitrate_ = DataRate::Zero();
  sum_utilization_factors_ = 0.0;
  target_framerate_fps_ = 0;
  buffer_level_bits_ = 0;
}

int64_t EncoderOvershootDetector::IdealFrameSizeBits() const {
  if (target_framerate_fps_ <= 0 || target_bitrate_ == DataRate::Zero()) {
    return 0;
  }

  // Current ideal frame size, based on the current target bitrate.
  return (target_bitrate_.bps() + target_framerate_fps_ / 2) /
         target_framerate_fps_;
}

void EncoderOvershootDetector::LeakBits(int64_t time_ms) {
  if (time_last_update_ms_ != -1 && target_bitrate_ > DataRate::Zero()) {
    int64_t time_delta_ms = time_ms - time_last_update_ms_;
    // Leak bits according to the current target bitrate.
    int64_t leaked_bits = std::min(
        buffer_level_bits_, (target_bitrate_.bps() * time_delta_ms) / 1000);
    buffer_level_bits_ -= leaked_bits;
  }
  time_last_update_ms_ = time_ms;
}

}  // namespace webrtc
