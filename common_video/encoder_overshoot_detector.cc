/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/include/encoder_overshoot_detector.h"

namespace webrtc {

EncoderOvershootDetector::EncoderOvershootDetector(int64_t window_size_ms)
    : time_last_update_ms_(-1),
      target_bitrate_(DataRate::Zero()),
      target_framerate_fps_(0),
      // Rate statistics using native bits and ms scale.
      overshoot_rate_(window_size_ms, 3000),
      buffer_level_bits_(0) {}

EncoderOvershootDetector::~EncoderOvershootDetector() = default;

void EncoderOvershootDetector::SetTargetRate(DataRate target_bitrate,
                                             int target_framerate_fps,
                                             int64_t time_ms) {
  if (target_bitrate_ == DataRate::Zero() && target_bitrate > target_bitrate_) {
    time_last_update_ms_ = time_ms;
    overshoot_rate_.Reset();
    buffer_level_bits_ = 0;
  }
  //  printf("New target rate: %ld bps, %d fps, buffer level = %ld bits\n",
  //      target_bitrate.bps(), target_framerate_fps, buffer_level_bits_);
  LeakBits(time_ms);
  target_bitrate_ = target_bitrate;
  target_framerate_fps_ = target_framerate_fps;
}

void EncoderOvershootDetector::OnEncodedFrame(size_t bytes, int64_t time_ms) {
  LeakBits(time_ms);
  int64_t ideal_frame_size = IdealFrameSizeBits();
  int64_t bitsum = (bytes * 8) + buffer_level_bits_;
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
  if (target_bitrate_ == DataRate::Zero()) {
    return 1.0;
  }
  LeakBits(time_ms);
  absl::optional<uint32_t> overshoot_rate_bps = overshoot_rate_.Rate(time_ms);
  if (overshoot_rate_bps) {
    return target_bitrate_.bps<double>() /
           (target_bitrate_.bps() - *overshoot_rate_bps);
  }
  return absl::nullopt;
}

void EncoderOvershootDetector::Reset() {
  time_last_update_ms_ = -1;
  target_bitrate_ = DataRate::Zero();
  target_framerate_fps_ = 0;
  overshoot_rate_.Reset();
  buffer_level_bits_ = 0;
}

int64_t EncoderOvershootDetector::IdealFrameSizeBits() const {
  if (target_framerate_fps_ <= 0) {
    return 0;
  }

  return target_bitrate_.bps_or(0) / target_framerate_fps_;
}

void EncoderOvershootDetector::LeakBits(int64_t time_ms) {
  if (time_last_update_ms_ != -1) {
    int64_t time_delta_ms = time_ms - time_last_update_ms_;
    int64_t leaked_bits = std::min(
        buffer_level_bits_, (target_bitrate_.bps() * time_delta_ms) / 1000);
    buffer_level_bits_ -= leaked_bits;
  }
  time_last_update_ms_ = time_ms;
}

}  // namespace webrtc
