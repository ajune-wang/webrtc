/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ENCODER_OVERSHOOT_DETECTOR_H_
#define VIDEO_ENCODER_OVERSHOOT_DETECTOR_H_

#include <deque>

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
  // Returns the average target bitrate, over the same window as
  // |overshoot_rate_|. Returns zero rate if SetTargetRate() has not been
  // called.
  DataRate GetAverageTargetBitrate(int64_t time_ms);
  void CullTargetBitrates(int64_t time_ms);

  const int64_t window_size_ms_;
  int64_t time_last_update_ms_;
  struct BitrateUpdate {
    BitrateUpdate(DataRate bitrate, int64_t update_time_ms)
        : bitrate(bitrate), update_time_ms(update_time_ms) {}
    DataRate bitrate;
    int64_t update_time_ms;
  };
  std::deque<BitrateUpdate> target_bitrate_updates_;
  int target_framerate_fps_;
  RateStatistics overshoot_rate_;
  int64_t buffer_level_bits_;
};

}  // namespace webrtc

#endif  // VIDEO_ENCODER_OVERSHOOT_DETECTOR_H_
