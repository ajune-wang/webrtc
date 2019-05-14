/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/report_block_data.h"

namespace webrtc {

ReportBlockData::ReportBlockData()
    : report_block_(),
      report_block_timestamp_utc_us_(0),
      last_rtt_ms_(0),
      min_rtt_ms_(0),
      max_rtt_ms_(0),
      sum_rtt_ms_(0),
      num_rtts_(0) {}

const RTCPReportBlock& ReportBlockData::report_block() const {
  return report_block_;
}

int64_t ReportBlockData::report_block_timestamp_utc_us() const {
  return report_block_timestamp_utc_us_;
}

int64_t ReportBlockData::last_rtt_ms() const {
  return last_rtt_ms_;
}

int64_t ReportBlockData::min_rtt_ms() const {
  return min_rtt_ms_;
}

int64_t ReportBlockData::max_rtt_ms() const {
  return max_rtt_ms_;
}

int64_t ReportBlockData::sum_rtt_ms() const {
  return sum_rtt_ms_;
}

size_t ReportBlockData::num_rtts() const {
  return num_rtts_;
}

void ReportBlockData::SetReportBlock(RTCPReportBlock report_block,
                                     int64_t report_block_timestamp_utc_us) {
  report_block_ = report_block;
  report_block_timestamp_utc_us_ = report_block_timestamp_utc_us;
}

void ReportBlockData::AddRoundTripTimeSample(int64_t rtt_ms) {
  if (rtt_ms > max_rtt_ms_)
    max_rtt_ms_ = rtt_ms;
  if (num_rtts_ == 0 || rtt_ms < min_rtt_ms_)
    min_rtt_ms_ = rtt_ms;
  last_rtt_ms_ = rtt_ms;
  sum_rtt_ms_ += rtt_ms;
  ++num_rtts_;
}

}  // namespace webrtc
