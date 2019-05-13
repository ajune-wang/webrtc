/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef STATS_DATA_REPORT_BLOCK_DATA_H_
#define STATS_DATA_REPORT_BLOCK_DATA_H_

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

struct ReportBlockData {
  RTCPReportBlock report_block;

  int64_t report_block_timestamp_us = 0;
  int64_t last_rtt_ms = 0;
  int64_t min_rtt_ms = 0;
  int64_t max_rtt_ms = 0;
  int64_t sum_rtt_ms = 0;
  size_t num_rtts = 0;
};

class ReportBlockDataObserver {
 public:
  virtual ~ReportBlockDataObserver() {}

  virtual void OnReportBlockDataUpdated(ReportBlockData report_block_data) = 0;
};

}  // namespace webrtc

#endif  // STATS_DATA_REPORT_BLOCK_DATA_H_
