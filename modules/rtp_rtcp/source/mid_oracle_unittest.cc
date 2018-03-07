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

namespace webrtc {

class MidOracleTest : public ::testing::Test {
 protected:
  MidOracleTest() : mid_oracle_("mid") {}

  void ReportRTCPFeedback(uint32_t ssrc, uint32_t seq_num) {
    RTCPReportBlock report_block;
    report_block.source_ssrc = ssrc;
    report_block.extended_highest_sequence_number = seq_num;
    report_blocks_.push_back(report_block);
    mid_oracle_.OnReceivedRtcpReportBlocks(report_blocks_);
  }

  ReportBlockList report_blocks_;
  MidOracle mid_oracle_;
};

// Test that the oracle says to send the MID until there is an RTCP
// acknowledgment for that SSRC.
TEST_F(MidOracleTest, SendMidUntilRtcpAcknowledgment) {
  constexpr uint32_t kSsrc = 52;
  constexpr uint32_t kOtherSsrc = 63;

  // Oracle should now say to send MID until it sees an RTCP acknowledgment for
  // that SSRC.
  mid_oracle_.SetSsrc(kSsrc);
  EXPECT_TRUE(mid_oracle_.send_mid());

  // Reports for a different SSRC should not change the MID sending status.
  ReportRTCPFeedback(kOtherSsrc, 100);
  EXPECT_TRUE(mid_oracle_.send_mid());

  // Report received for the sending SSRC, stop sending MID.
  ReportRTCPFeedback(kSsrc, 200);
  EXPECT_FALSE(mid_oracle_.send_mid());

  // Changing the SSRC will cause it to say to send the MID again.
  mid_oracle_.SetSsrc(kOtherSsrc);
  EXPECT_TRUE(mid_oracle_.send_mid());
}

// Test that the oracle works in the same way for rtx streams.
TEST_F(MidOracleTest, SendMidUntilRtcpAcknowledgmentRtx) {
  constexpr uint32_t kSsrcRtx = 53;
  constexpr uint32_t kOtherSsrcRtx = 64;

  mid_oracle_.SetSsrcRtx(kSsrcRtx);
  EXPECT_TRUE(mid_oracle_.send_mid_rtx());

  ReportRTCPFeedback(kOtherSsrcRtx, 100);
  EXPECT_TRUE(mid_oracle_.send_mid_rtx());

  ReportRTCPFeedback(kSsrcRtx, 200);
  EXPECT_FALSE(mid_oracle_.send_mid_rtx());

  mid_oracle_.SetSsrcRtx(kOtherSsrcRtx);
  EXPECT_TRUE(mid_oracle_.send_mid_rtx());
}

}  // namespace webrtc
