/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_MID_ORACLE_H_
#define MODULES_RTP_RTCP_SOURCE_MID_ORACLE_H_

#include <stdint.h>
#include <string>

#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// The MidOracle instructs an RTPSender to send the MID header extension on a
// new SSRC stream until it receives an RTCP acknowledgment for that stream
// (which implies that the remote side is able to demultiplex it and can
// remember the MID --> SSRC mapping).
class MidOracle {
 public:
  explicit MidOracle(const std::string& mid);
  ~MidOracle();

  const std::string& mid() const { return mid_; }

  bool send_mid() const { return send_mid_; }

  void SetSsrc(uint32_t ssrc) {
    ssrc_ = ssrc;
    send_mid_ = true;
  }

  void OnReceivedRtcpReportBlocks(const ReportBlockList& report_blocks);

 private:
  const std::string mid_;
  bool send_mid_ = false;
  uint32_t ssrc_ = 0;

  RTC_DISALLOW_COPY_AND_ASSIGN(MidOracle);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_MID_ORACLE_H_
