/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_INCLUDE_ENCODER_OVERSHOOT_DETECTOR_H_
#define COMMON_VIDEO_INCLUDE_ENCODER_OVERSHOOT_DETECTOR_H_

#include "absl/types/optional.h"
#include "api/units/data_rate.h"
#include "rtc_base/rate_statistics.h"

namespace webrtc {

class EncoderOvershootDetector {
 public:
  explicit EncoderOvershootDetector(int64_t window_size_ms);
  ~EncoderOvershootDetector();

  void SetTargetRate(DataRate target_bitrate,
                     int target_framerate_fps,
                     int64_t time_ms);
  void OnEncodedFrame(size_t bytes, int64_t time_ms);
  absl::optional<double> GetUtilizationFactor(int64_t time_ms);
  void Reset();

 private:
  int64_t IdealFrameSizeBits() const;
  void LeakBits(int64_t time_ms);

  int64_t time_last_update_ms_;
  DataRate target_bitrate_;
  int target_framerate_fps_;
  RateStatistics overshoot_rate_;
  int64_t buffer_level_bits_;
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_INCLUDE_ENCODER_OVERSHOOT_DETECTOR_H_
