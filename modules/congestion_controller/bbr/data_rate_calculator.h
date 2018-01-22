/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_BBR_DATA_RATE_CALCULATOR_H_
#define MODULES_CONGESTION_CONTROLLER_BBR_DATA_RATE_CALCULATOR_H_

#include <deque>
#include "network_control/include/network_units.h"

namespace webrtc {
namespace bbr {
class DataRateCalculator {
 public:
  struct Result {
    TimeDelta ack_timespan;
    TimeDelta send_timespan;
    DataSize acked_data;
  };
  DataRateCalculator();
  ~DataRateCalculator();
  void push_back(DataSize size_delta, Timestamp send_time, Timestamp ack_time);
  void clear_old(Timestamp excluding_end);

  Result GetRatesByAckTime(Timestamp covered_start, Timestamp including_end);

 private:
  struct Sample {
    Timestamp ack_time;
    Timestamp send_time;
    DataSize size_delta;
    DataSize size_sum;
  };
  std::deque<Sample> samples_;
};
}  // namespace bbr
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_BBR_DATA_RATE_CALCULATOR_H_
