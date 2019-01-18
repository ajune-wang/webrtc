/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EXPERIMENTS_VIDEO_RATE_CONTROL_EXPERIMENTS_H_
#define RTC_BASE_EXPERIMENTS_VIDEO_RATE_CONTROL_EXPERIMENTS_H_

#include "absl/types/optional.h"
#include "rtc_base/experiments/webrtc_key_value_config.h"

namespace webrtc {

class VideoRateControlExperiments final {
 public:
  ~VideoRateControlExperiments();
  VideoRateControlExperiments(VideoRateControlExperiments&&);

  static VideoRateControlExperiments ParseFromFieldTrials();
  static VideoRateControlExperiments ParseFromKeyValueConfig(
      const WebRtcKeyValueConfig* const key_value_config);

  // When CongestionWindowPushback is enabled, the pacer is oblivious to
  // the congestion window. The relation between outstanding data and
  // the congestion window affects encoder allocations directly.
  absl::optional<int64_t> GetCongestionWindowParameter() const;
  absl::optional<uint32_t> GetCongestionWindowPushbackParameter() const;

 private:
  VideoRateControlExperiments(
      const WebRtcKeyValueConfig* const key_value_config);

  absl::optional<int64_t> congestion_window_;
  absl::optional<uint32_t> congestion_window_pushback_;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_VIDEO_RATE_CONTROL_EXPERIMENTS_H_
