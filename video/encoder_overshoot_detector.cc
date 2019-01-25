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
      target_bitsum_x1000_(0),
      target_framerate_fps_(0),
      // Rate statistics using native bits and ms scale.
      overshoot_rate_(window_size_ms, 1000),
      buffer_level_bits_(0) {}

EncoderOvershootDetector::~EncoderOvershootDetector() = default;

void EncoderOvershootDetector::SetTargetRate(DataRate target_bitrate,
                                             int target_framerate_fps,
                                             int64_t time_ms) {
  if ((target_bitrate_updates_.empty() ||
       target_bitrate_updates_.back().bitrate == DataRate::Zero()) &&
      target_bitrate != DataRate::Zero()) {
    // Stream was just enabled, reset state.
    time_last_update_ms_ = time_ms;
    overshoot_rate_.Reset();
    target_bitsum_x1000_ = 0;
    target_bitrate_updates_.clear();
    buffer_level_bits_ = 0;
  }

  LeakBits(time_ms);

  // Make sure there are no overlapping target bitrate segments.
  int64_t end_time_ms = time_ms;
  if (!target_bitrate_updates_.empty()) {
    end_time_ms =
        std::max(time_ms, target_bitrate_updates_.back().update_time_ms);
  }
  if (!target_bitrate_updates_.empty()) {
    const BitrateUpdate& last = target_bitrate_updates_.back();
    target_bitsum_x1000_ +=
        (end_time_ms - last.update_time_ms) * last.bitrate.bps();
  }
  target_bitrate_updates_.emplace_back(target_bitrate, end_time_ms);

  target_framerate_fps_ = target_framerate_fps;
}

void EncoderOvershootDetector::OnEncodedFrame(size_t bytes, int64_t time_ms) {
  LeakBits(time_ms);
  const int64_t ideal_frame_size = IdealFrameSizeBits();
  const int64_t bitsum = (bytes * 8) + buffer_level_bits_;
  int64_t overshoot_bits = 0;
  if (bitsum > ideal_frame_size) {
    overshoot_bits = std::min(buffer_level_bits_, bitsum - ideal_frame_size);
    overshoot_rate_.Update(overshoot_bits, time_ms);
  }
  buffer_level_bits_ -= overshoot_bits;
  buffer_level_bits_ += bytes * 8;
}

absl::optional<double> EncoderOvershootDetector::GetUtilizationFactor(
    int64_t time_ms) {
  const DataRate target_bitrate = GetAverageTargetBitrate(time_ms);
  if (target_bitrate == DataRate::Zero()) {
    return 1.0;
  }

  LeakBits(time_ms);
  const absl::optional<uint32_t> overshoot_rate_bps =
      overshoot_rate_.Rate(time_ms);
  if (overshoot_rate_bps) {
    return target_bitrate.bps<double>() /
           (target_bitrate.bps() - overshoot_rate_bps.value());
  }

  return absl::nullopt;
}

void EncoderOvershootDetector::Reset() {
  time_last_update_ms_ = -1;
  target_bitrate_updates_.clear();
  target_framerate_fps_ = 0;
  overshoot_rate_.Reset();
  buffer_level_bits_ = 0;
}

int64_t EncoderOvershootDetector::IdealFrameSizeBits() const {
  if (target_framerate_fps_ <= 0 || target_bitrate_updates_.empty()) {
    return 0;
  }

  // Current ideal frame size, based on the current target bitrate.
  return target_bitrate_updates_.back().bitrate.bps() / target_framerate_fps_;
}

void EncoderOvershootDetector::LeakBits(int64_t time_ms) {
  if (time_last_update_ms_ != -1 && !target_bitrate_updates_.empty()) {
    int64_t time_delta_ms = time_ms - time_last_update_ms_;
    // Leak bits according to the current target bitrate.
    int64_t leaked_bits = std::min(
        buffer_level_bits_,
        (target_bitrate_updates_.back().bitrate.bps() * time_delta_ms) / 1000);
    buffer_level_bits_ -= leaked_bits;
  }
  time_last_update_ms_ = time_ms;
}

DataRate EncoderOvershootDetector::GetAverageTargetBitrate(int64_t time_ms) {
  if (target_bitrate_updates_.empty() ||
      target_bitrate_updates_.front().update_time_ms == time_ms) {
    return DataRate::Zero();
  }

  RTC_DCHECK_GE(time_ms, target_bitrate_updates_.back().update_time_ms);

  // Cull BitrateUpdate items that are too old.
  while (target_bitrate_updates_.size() > 1) {
    const BitrateUpdate& first = target_bitrate_updates_.front();
    BitrateUpdate& second = target_bitrate_updates_[1];
    if (time_ms - second.update_time_ms > window_size_ms_) {
      // Second to last entry starts outside limit, remove the first entirely.
      target_bitsum_x1000_ -=
          (second.update_time_ms - first.update_time_ms) * first.bitrate.bps();
      RTC_DCHECK_GE(target_bitsum_x1000_, 0);

      target_bitrate_updates_.pop_front();
      continue;
    }

    break;
  }

  // If the first entry is partially outside the window, clip it.
  BitrateUpdate& first = target_bitrate_updates_.front();
  if ((time_ms - first.update_time_ms) > window_size_ms_) {
    int64_t new_update_time_ms = time_ms - window_size_ms_;
    target_bitsum_x1000_ -=
        (new_update_time_ms - first.update_time_ms) * first.bitrate.bps();
    RTC_DCHECK_GE(target_bitsum_x1000_, 0);

    first.update_time_ms = new_update_time_ms;
  }

  // Add the bitrate from the last update until now.
  const BitrateUpdate& last = target_bitrate_updates_.back();
  return DataRate::bps((target_bitsum_x1000_ +
                        (time_ms - last.update_time_ms) * last.bitrate.bps()) /
                       window_size_ms_);
}

}  // namespace webrtc
