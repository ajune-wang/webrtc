/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/mid_oracle.h"

#include <string>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

MidOracle::MidOracle(const std::string& mid)
    : mid_(mid),
      send_mid_(false),
      ssrc_(0),
      send_mid_rtx_(false),
      ssrc_rtx_(0) {}

MidOracle::~MidOracle() = default;

void MidOracle::OnReceivedRtcpReportBlocks(
    const ReportBlockList& report_blocks) {
  rtc::CritScope lock(&crit_sect_);
  if (!send_mid_ && !send_mid_rtx_) {
    return;
  }
  for (const RTCPReportBlock& report_block : report_blocks) {
    if (send_mid_ && report_block.source_ssrc == ssrc_) {
      send_mid_ = false;
    }
    if (send_mid_rtx_ && report_block.source_ssrc == ssrc_rtx_) {
      send_mid_rtx_ = false;
    }
  }
}

}  // namespace webrtc
