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

#include "rtc_base/logging.h"
#include "test/gtest.h"

using webrtc::RTCPReportBlock;
using webrtc::MidOracle;

namespace {

RTCPReportBlock ReportBlockWithSourceSsrc(uint32_t ssrc) {
  RTCPReportBlock report_block;
  report_block.source_ssrc = ssrc;
  return report_block;
}

// Test that the oracle says to send the MID until there is an RTCP
// acknowledgment for that SSRC.
TEST(MidOracleTest, SendMidUntilRtcpAcknowledgment) {
  constexpr uint32_t kSsrc = 52;
  constexpr uint32_t kOtherSsrc = 63;

  MidOracle mid_oracle("mid");

  // Oracle should now say to send MID until it sees an RTCP acknowledgment for
  // that SSRC.
  mid_oracle.SetSsrc(kSsrc);
  EXPECT_TRUE(mid_oracle.send_mid());

  // Reports for a different SSRC should not change the MID sending status.
  mid_oracle.OnReceivedRtcpReportBlocks(
      {ReportBlockWithSourceSsrc(kOtherSsrc)});
  EXPECT_TRUE(mid_oracle.send_mid());

  // Report received for the sending SSRC, stop sending MID.
  mid_oracle.OnReceivedRtcpReportBlocks({ReportBlockWithSourceSsrc(kSsrc)});
  EXPECT_FALSE(mid_oracle.send_mid());

  // Changing the SSRC will cause it to say to send the MID again.
  mid_oracle.SetSsrc(kOtherSsrc);
  EXPECT_TRUE(mid_oracle.send_mid());
}

}  // namespace
