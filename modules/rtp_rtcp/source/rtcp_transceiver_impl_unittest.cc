/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_transceiver_impl.h"

#include <vector>

#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/source/time_util.h"
#include "rtc_base/event.h"
#include "rtc_base/fakeclock.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/timeutils.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/rtcp_packet_parser.h"

namespace {

using ::testing::Invoke;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::_;
using ::webrtc::CompactNtp;
using ::webrtc::CompactNtpRttToMs;
using ::webrtc::MockTransport;
using ::webrtc::RtcpTransceiver;
using ::webrtc::RtcpTransceiverImpl;
using ::webrtc::rtcp::ReportBlock;
using ::webrtc::test::RtcpPacketParser;

class MockReceiveStatisticsProvider : public webrtc::ReceiveStatisticsProvider {
 public:
  MOCK_METHOD1(RtcpReportBlocks, std::vector<ReportBlock>(size_t));
};

using NullTransport = ::testing::NiceMock<MockTransport>;

TEST(RtcpTransceiverImplTest, PeriodicallySendsReceiverReport) {
  rtc::TaskQueue queue("rtcp");
  const uint32_t kSenderSsrc = 1234;
  const uint32_t kMediaSsrc = 3456;
  rtc::Event success(false, false);
  size_t sent_rtcp_packets = 0;
  MockReceiveStatisticsProvider receive_statistics;
  std::vector<ReportBlock> report_blocks(1);
  report_blocks[0].SetMediaSsrc(kMediaSsrc);
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillRepeatedly(Return(report_blocks));

  MockTransport outgoing_transport;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillRepeatedly(Invoke([&](const uint8_t* buffer, size_t size) {
        EXPECT_TRUE(queue.IsCurrent());
        RtcpPacketParser rtcp_parser;
        EXPECT_TRUE(rtcp_parser.Parse(buffer, size));
        EXPECT_EQ(rtcp_parser.receiver_report()->num_packets(), 1);
        EXPECT_EQ(rtcp_parser.receiver_report()->sender_ssrc(), kSenderSsrc);
        EXPECT_THAT(rtcp_parser.receiver_report()->report_blocks(),
                    SizeIs(report_blocks.size()));
        EXPECT_EQ(
            rtcp_parser.receiver_report()->report_blocks()[0].source_ssrc(),
            kMediaSsrc);
        if (++sent_rtcp_packets >= 2)  // i.e. EXPECT_CALL().Times(AtLeast(2))
          success.Set();
        return true;
      }));

  std::unique_ptr<RtcpTransceiverImpl> rtcp_transceiver;

  queue.PostTask([&] {
    RtcpTransceiver::Configuration config;
    config.feedback_ssrc = kSenderSsrc;
    config.outgoing_transport = &outgoing_transport;
    config.receive_statistics = &receive_statistics;
    config.min_periodic_report_ms = 10;

    rtcp_transceiver = rtc::MakeUnique<RtcpTransceiverImpl>(config);
  });
  EXPECT_FALSE(success.Wait(/*milliseconds=*/0));
  EXPECT_TRUE(success.Wait(/*milliseconds=*/25));
  rtc::Event done(false, false);
  queue.PostTask([&] {
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(100));
}

TEST(RtcpTransceiverImplTest, ForceSendReportAsap) {
  MockTransport outgoing_transport;
  RtcpPacketParser rtcp_parser;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillOnce(Invoke(&rtcp_parser, &RtcpPacketParser::Parse));

  RtcpTransceiver::Configuration config;
  config.outgoing_transport = &outgoing_transport;
  config.schedule_periodic_reports = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.ForceSendReport();

  EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
}

TEST(RtcpTransceiverImplTest, AttachSdesWhenCnameSpecified) {
  const uint32_t kSenderSsrc = 1234;
  const std::string kCname = "sender";
  MockTransport outgoing_transport;
  RtcpPacketParser rtcp_parser;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillOnce(Invoke(&rtcp_parser, &RtcpPacketParser::Parse));

  RtcpTransceiver::Configuration config;
  config.feedback_ssrc = kSenderSsrc;
  config.cname = kCname;
  config.outgoing_transport = &outgoing_transport;
  config.schedule_periodic_reports = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.ForceSendReport();

  EXPECT_EQ(rtcp_parser.sdes()->num_packets(), 1);
  EXPECT_THAT(rtcp_parser.sdes()->chunks(), SizeIs(1));
  EXPECT_EQ(rtcp_parser.sdes()->chunks()[0].ssrc, kSenderSsrc);
  EXPECT_EQ(rtcp_parser.sdes()->chunks()[0].cname, kCname);
}

TEST(RtcpTransceiverImplTest, CalculatesDelaySinceLastSenderReport) {
  rtc::ScopedFakeClock clock;
  const uint32_t remote_ssrc = 4321;
  const webrtc::NtpTime remote_ntp(0x9876543211);
  MockTransport outgoing_transport;
  testing::NiceMock<MockReceiveStatisticsProvider> receive_statistics;
  RtcpTransceiver::Configuration config;
  config.schedule_periodic_reports = false;
  config.outgoing_transport = &outgoing_transport;
  config.receive_statistics = &receive_statistics;
  std::vector<ReportBlock> rbs(1);
  rbs[0].SetMediaSsrc(remote_ssrc);
  ON_CALL(receive_statistics, RtcpReportBlocks(_)).WillByDefault(Return(rbs));
  RtcpPacketParser rtcp_parser;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillRepeatedly(Invoke(&rtcp_parser, &RtcpPacketParser::Parse));

  RtcpTransceiverImpl rtcp_transceiver(config);
  webrtc::rtcp::SenderReport sr;
  sr.SetSenderSsrc(remote_ssrc);
  sr.SetNtp(remote_ntp);
  auto raw_packet = sr.Build();
  rtcp_transceiver.ReceivePacket(raw_packet);
  clock.AdvanceTime(rtc::TimeDelta::FromMilliseconds(100));
  rtcp_transceiver.ForceSendReport();

  EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  ASSERT_GT(rtcp_parser.receiver_report()->report_blocks().size(), 0u);
  EXPECT_EQ(rtcp_parser.receiver_report()->report_blocks()[0].last_sr(),
            CompactNtp(remote_ntp));
  EXPECT_NEAR(CompactNtpRttToMs(rtcp_parser.receiver_report()
                                    ->report_blocks()[0]
                                    .delay_since_last_sr()),
              100, 1);
}

}  // namespace
