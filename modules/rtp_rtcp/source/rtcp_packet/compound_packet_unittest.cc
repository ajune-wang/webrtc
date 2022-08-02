/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/compound_packet.h"

#include <memory>
#include <utility>

#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/fir.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {
namespace rtcp {
namespace {

using ::webrtc::test::RtcpPacketParser;

constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint32_t kRemoteSsrc = 0x23456789;
constexpr uint8_t kSeqNo = 13;

TEST(RtcpCompoundPacketTest, AppendPacketsAndBuild) {
  auto fir = std::make_unique<Fir>();
  fir->AddRequestTo(kRemoteSsrc, kSeqNo);
  ReportBlock rb;
  auto rr = std::make_unique<ReceiverReport>();
  rr->SetSenderSsrc(kSenderSsrc);
  EXPECT_TRUE(rr->AddReportBlock(rb));

  CompoundPacket compound;
  compound.Append(std::move(rr));
  compound.Append(std::move(fir));
  rtc::Buffer packet = compound.Build();

  RtcpPacketParser parser;
  parser.Parse(packet.data(), packet.size());
  EXPECT_EQ(1, parser.receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.receiver_report()->sender_ssrc());
  EXPECT_EQ(1u, parser.receiver_report()->report_blocks().size());
  EXPECT_EQ(1, parser.fir()->num_packets());
}

}  // namespace
}  // namespace rtcp
}  // namespace webrtc
