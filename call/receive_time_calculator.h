/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_RECEIVE_TIME_CALCULATOR_H_
#define CALL_RECEIVE_TIME_CALCULATOR_H_

#include <stdint.h>
#include <memory>

namespace webrtc {
class ReceiveTimeCalculator {
 public:
  static std::unique_ptr<ReceiveTimeCalculator> CreateFromFieldTrial();

  ReceiveTimeCalculator(int64_t min_delta_diff_ms, int64_t max_delta_diff_ms);
  int64_t ReconcileReceiveTimes(int64_t packet_time_us_, int64_t safe_time_us_);

 private:
  const int64_t min_delta_diff_us_;
  const int64_t max_delta_diff_us_;
  bool receive_time_offset_set_ = false;
  int64_t receive_time_offset_us_ = 0;
  int64_t last_packet_time_us_ = 0;
  int64_t last_safe_time_us_ = 0;
};
}  // namespace webrtc
#endif  // CALL_RECEIVE_TIME_CALCULATOR_H_
