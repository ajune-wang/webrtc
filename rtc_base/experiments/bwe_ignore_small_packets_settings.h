/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_BWE_IGNORE_SMALL_PACKETS_SETTINGS_H_
#define RTC_BASE_EXPERIMENTS_BWE_IGNORE_SMALL_PACKETS_SETTINGS_H_

#include "api/transport/webrtc_key_value_config.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

class BweIgnoreSmallPacketsSettings {
 public:
  static BweIgnoreSmallPacketsSettings ParseFromFieldTrials();
  explicit BweIgnoreSmallPacketsSettings(
      const WebRtcKeyValueConfig* key_value_config);

  double min_fraction_large_packets() const {
    return min_fraction_large_packets_.Get();
  }
  size_t large_packet_size() const { return large_packet_size_.Get(); }
  size_t ignored_size() const { return ignored_size_.Get(); }

 private:
  FieldTrialParameter<double> min_fraction_large_packets_;
  FieldTrialParameter<int> large_packet_size_;
  FieldTrialParameter<int> ignored_size_;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_BWE_IGNORE_SMALL_PACKETS_SETTINGS_H_
