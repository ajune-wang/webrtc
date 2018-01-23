/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/bbr/data_rate_calculator.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace bbr {

DataRateCalculator::DataRateCalculator() {}

DataRateCalculator::~DataRateCalculator() {}

void DataRateCalculator::push_back(DataSize size_delta,
                                   Timestamp send_time,
                                   Timestamp ack_time) {
  Sample new_sample;
  new_sample.ack_time = ack_time;
  new_sample.send_time = send_time;
  new_sample.size_delta = size_delta;
  new_sample.size_sum = size_delta;
  if (!samples_.empty())
    new_sample.size_sum += samples_.back().size_sum;
  samples_.push_back(new_sample);
}

void DataRateCalculator::clear_old(Timestamp excluding_end) {
  while (!samples_.empty() && samples_.front().ack_time < excluding_end) {
    samples_.pop_front();
  }
}

DataRateCalculator::Result DataRateCalculator::GetRatesByAckTime(
    Timestamp covered_start,
    Timestamp including_end) {
  Result res;
  const Sample* last_before = nullptr;
  const Sample* first_end_or_after = nullptr;
  for (const auto& sample : samples_) {
    if (sample.ack_time < covered_start)
      last_before = &sample;
    else if (sample.ack_time >= including_end)
      first_end_or_after = &sample;
  }
  if (last_before != nullptr && first_end_or_after != nullptr) {
    res.acked_data = first_end_or_after->size_sum - last_before->size_sum;
    res.send_timespan = first_end_or_after->send_time - last_before->send_time;
    res.ack_timespan = first_end_or_after->ack_time - last_before->ack_time;
  }
  return res;
}

}  // namespace bbr
}  // namespace webrtc
