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
namespace network {
namespace bbr {
class DataRateCalculator {
 public:
  struct Result {
    units::TimeDelta ack_timespan;
    units::TimeDelta send_timespan;
    units::DataSize acked_data;
  };
  DataRateCalculator();
  ~DataRateCalculator();
  void push_back(units::DataSize size_delta,
                 units::Timestamp send_time,
                 units::Timestamp ack_time);
  void clear_old(units::Timestamp excluding_end);

  Result GetRatesByAckTime(units::Timestamp covered_start,
                           units::Timestamp including_end);

 private:
  struct Sample {
    units::Timestamp ack_time;
    units::Timestamp send_time;
    units::DataSize size_delta;
    units::DataSize size_sum;
  };
  std::deque<Sample> samples_;
};
}  // namespace bbr
}  // namespace network
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_BBR_DATA_RATE_CALCULATOR_H_
