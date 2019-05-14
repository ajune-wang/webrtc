/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_REPORT_BLOCK_DATA_H_
#define MODULES_RTP_RTCP_INCLUDE_REPORT_BLOCK_DATA_H_

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class ReportBlockData {
 public:
  ReportBlockData();

  const RTCPReportBlock& report_block() const;
  int64_t report_block_timestamp_utc_us() const;
  int64_t last_rtt_ms() const;
  int64_t min_rtt_ms() const;
  int64_t max_rtt_ms() const;
  int64_t sum_rtt_ms() const;
  size_t num_rtts() const;

  void SetReportBlock(RTCPReportBlock report_block,
                      int64_t report_block_timestamp_utc_us);
  void AddRoundTripTimeSample(int64_t rtt_ms);

 private:
  RTCPReportBlock report_block_;
  int64_t report_block_timestamp_utc_us_;

  int64_t last_rtt_ms_;
  int64_t min_rtt_ms_;
  int64_t max_rtt_ms_;
  int64_t sum_rtt_ms_;
  size_t num_rtts_;
};

class ReportBlockDataObserver {
 public:
  virtual ~ReportBlockDataObserver() {}

  virtual void OnReportBlockDataUpdated(ReportBlockData report_block_data) = 0;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_REPORT_BLOCK_DATA_H_
