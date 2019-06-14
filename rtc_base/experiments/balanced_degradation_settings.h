/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EXPERIMENTS_BALANCED_DEGRADATION_SETTINGS_H_
#define RTC_BASE_EXPERIMENTS_BALANCED_DEGRADATION_SETTINGS_H_

#include <vector>

#include "absl/types/optional.h"
#include "api/video_codecs/video_encoder.h"

namespace webrtc {

class BalancedDegradationSettings {
 public:
  BalancedDegradationSettings();
  ~BalancedDegradationSettings();

  struct Config {
    Config();
    Config(int pixels,
           int fps,
           int vp8_qp_high,
           int h264_qp_high,
           int generic_qp_high);

    bool operator==(const Config& o) const {
      return pixels == o.pixels && fps == o.fps &&
             vp8_qp_high == o.vp8_qp_high && h264_qp_high == o.h264_qp_high &&
             generic_qp_high == o.generic_qp_high;
    }

    int pixels = 0;           // The video frame size.
    int fps = 0;              // The framerate to be used if the frame size is
                              // less than or equal to |pixels|.
    int vp8_qp_high = 0;      // VP8: high QP threshold.
    int h264_qp_high = 0;     // H264: high QP threshold.
    int generic_qp_high = 0;  // Generic: high QP threshold.
  };

  // Returns configurations from field trial on success (default on failure).
  std::vector<Config> GetConfigs() const;

  // Gets the min/max framerate from |configs_| based on |pixels|.
  int MinFps(int pixels) const;
  int MaxFps(int pixels) const;

  // Gets the high QP threshold based on codec |type| and |pixels|.
  absl::optional<int> QpHighThreshold(VideoCodecType type, int pixels) const;

 private:
  absl::optional<int> Vp8QpHigh(int pixels) const;
  absl::optional<int> H264QpHigh(int pixels) const;
  absl::optional<int> GenericQpHigh(int pixels) const;

  std::vector<Config> configs_;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_BALANCED_DEGRADATION_SETTINGS_H_
