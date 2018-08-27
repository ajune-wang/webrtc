/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_RTT_MULT_EXPERIMENT_H_
#define RTC_BASE_EXPERIMENTS_RTT_MULT_EXPERIMENT_H_

namespace webrtc {
class RttMultExperiment {
 public:
  float rtt_mult_setting;  // value of rtt_mult pulled from experiment

  // Returns true if the experiment is enabled.
  static bool RttMultEnabled();

  // Returns rtt_mult value from field trial.
  // static absl::optional<float> GetRttMult();
  static float GetRttMult();
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_RTT_MULT_EXPERIMENT_H_
