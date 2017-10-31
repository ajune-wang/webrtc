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
#include "rtc_base/ptr_util.h"
#include "rtc_base/task_queue.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/rtcp_packet_parser.h"

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SizeIs;
using ::webrtc::MockTransport;
using ::webrtc::RtcpTransceiverConfig;
using ::webrtc::RtcpTransceiverImpl;
using ::webrtc::rtcp::ReportBlock;
using ::webrtc::test::RtcpPacketParser;
using ::webrtc::SimulatedClock;

class MockReceiveStatisticsProvider : public webrtc::ReceiveStatisticsProvider {
 public:
  MOCK_METHOD1(RtcpReportBlocks, std::vector<ReportBlock>(size_t));
};

// Helper to wait for an rtcp packet produced on a different thread/task queue.
class FakeRtcpTransport : public webrtc::Transport {
 public:
  FakeRtcpTransport() : sent_rtcp_(false, false) {}
  bool SendRtcp(const uint8_t* data, size_t size) override {
    sent_rtcp_.Set();
    return true;
  }
  bool SendRtp(const uint8_t*, size_t, const webrtc::PacketOptions&) override {
    ADD_FAILURE() << "RtcpTransciver shouldn't send rtp packets.";
    return true;
  }

  // Returns true if packet was received by this transport before timeout,
  bool WaitPacket(int64_t timeout_ms) { return sent_rtcp_.Wait(timeout_ms); }

 private:
  rtc::Event sent_rtcp_;
};

// Posting delayed tasks doesn't promise high precision.
constexpr int64_t kTaskQueuePrecisionMs = 15;

TEST(RtcpTransceiverImplTest, DelaysSendingFirstCompondPacket) {
  rtc::TaskQueue queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.outgoing_transport = &transport;
  config.initial_report_delay_ms = 10;
  config.task_queue = &queue;
  rtc::Optional<RtcpTransceiverImpl> rtcp_transceiver;

  int64_t started_ms = rtc::TimeMillis();
  queue.PostTask([&] { rtcp_transceiver.emplace(config); });
  EXPECT_TRUE(transport.WaitPacket(config.initial_report_delay_ms +
                                   kTaskQueuePrecisionMs));

  EXPECT_GE(rtc::TimeMillis() - started_ms, config.initial_report_delay_ms);

  // Cleanup.
  rtc::Event done(false, false);
  queue.PostTask([&] {
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(/*milliseconds=*/100));
}

TEST(RtcpTransceiverImplTest, PeriodicallySendsPackets) {
  rtc::TaskQueue queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.outgoing_transport = &transport;
  config.initial_report_delay_ms = 0;
  config.report_period_ms = 10;
  config.task_queue = &queue;
  rtc::Optional<RtcpTransceiverImpl> rtcp_transceiver;
  queue.PostTask([&] { rtcp_transceiver.emplace(config); });

  EXPECT_TRUE(transport.WaitPacket(config.initial_report_delay_ms +
                                   kTaskQueuePrecisionMs));
  int64_t time_of_1st_packet_ms = rtc::TimeMillis();
  EXPECT_TRUE(
      transport.WaitPacket(config.report_period_ms + kTaskQueuePrecisionMs));
  int64_t time_of_2nd_packet_ms = rtc::TimeMillis();

  EXPECT_GE(time_of_2nd_packet_ms - time_of_1st_packet_ms,
            config.report_period_ms);

  // Cleanup.
  rtc::Event done(false, false);
  queue.PostTask([&] {
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(/*milliseconds=*/100));
}

TEST(RtcpTransceiverImplTest, SendCompoundPacketDelaysPeriodicSendPackets) {
  rtc::TaskQueue queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.outgoing_transport = &transport;
  config.initial_report_delay_ms = 0;
  config.report_period_ms = 10;
  config.task_queue = &queue;
  rtc::Optional<RtcpTransceiverImpl> rtcp_transceiver;
  queue.PostTask([&] { rtcp_transceiver.emplace(config); });

  // Wait for first packet.
  EXPECT_TRUE(transport.WaitPacket(config.initial_report_delay_ms +
                                   kTaskQueuePrecisionMs));
  // Wait half-period time for next one - it shouldn't be sent.
  EXPECT_FALSE(transport.WaitPacket(config.report_period_ms / 2));
  // Send packet now.
  queue.PostTask([&] { rtcp_transceiver->SendCompoundPacket(); });
  EXPECT_TRUE(transport.WaitPacket(/*timeout_ms=*/1));
  int64_t time_of_non_periodic_packet_ms = rtc::TimeMillis();
  // Next packet should be sent at least after period_ms.
  EXPECT_TRUE(
      transport.WaitPacket(config.report_period_ms + kTaskQueuePrecisionMs));
  EXPECT_GE(rtc::TimeMillis() - time_of_non_periodic_packet_ms,
            config.report_period_ms);

  // Cleanup.
  rtc::Event done(false, false);
  queue.PostTask([&] {
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(/*milliseconds=*/100));
}

TEST(RtcpTransceiverImplTest, SendsMinimalCompoundPacket) {
  const uint32_t kSenderSsrc = 12345;
  MockTransport outgoing_transport;
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  config.cname = "cname";
  config.outgoing_transport = &outgoing_transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  RtcpPacketParser rtcp_parser;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillOnce(Invoke(&rtcp_parser, &RtcpPacketParser::Parse));
  rtcp_transceiver.SendCompoundPacket();

  // Minimal compound RTCP packet contains sender or receiver report and sdes
  // with cname.
  ASSERT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  EXPECT_EQ(rtcp_parser.receiver_report()->sender_ssrc(), kSenderSsrc);
  ASSERT_GT(rtcp_parser.sdes()->num_packets(), 0);
  ASSERT_EQ(rtcp_parser.sdes()->chunks().size(), 1u);
  EXPECT_EQ(rtcp_parser.sdes()->chunks()[0].ssrc, kSenderSsrc);
  EXPECT_EQ(rtcp_parser.sdes()->chunks()[0].cname, config.cname);
}

TEST(RtcpTransceiverImplTest, ReceiverReportUsesReceiveStatistics) {
  const uint32_t kSenderSsrc = 12345;
  const uint32_t kMediaSsrc = 54321;
  MockTransport outgoing_transport;
  RtcpPacketParser rtcp_parser;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillOnce(Invoke(&rtcp_parser, &RtcpPacketParser::Parse));

  MockReceiveStatisticsProvider receive_statistics;
  std::vector<ReportBlock> report_blocks(1);
  report_blocks[0].SetMediaSsrc(kMediaSsrc);
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillRepeatedly(Return(report_blocks));

  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  config.outgoing_transport = &outgoing_transport;
  config.receive_statistics = &receive_statistics;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();

  ASSERT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  EXPECT_EQ(rtcp_parser.receiver_report()->sender_ssrc(), kSenderSsrc);
  ASSERT_THAT(rtcp_parser.receiver_report()->report_blocks(),
              SizeIs(report_blocks.size()));
  EXPECT_EQ(rtcp_parser.receiver_report()->report_blocks()[0].source_ssrc(),
            kMediaSsrc);
}

TEST(RtcpTransceiverImplTest, CalculatesDelaySinceLastSenderReport) {
  const uint32_t kRemoteSsrc = 4321;
  SimulatedClock clock(11);
  MockTransport outgoing_transport;
  std::vector<ReportBlock> report_blocks(1);
  report_blocks[0].SetMediaSsrc(kRemoteSsrc);
  MockReceiveStatisticsProvider receive_statistics;
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillOnce(Return(report_blocks));

  RtcpTransceiverConfig config;
  config.schedule_periodic_compound_packets = false;
  config.outgoing_transport = &outgoing_transport;
  config.receive_statistics = &receive_statistics;
  config.clock = &clock;
  RtcpTransceiverImpl rtcp_transceiver(config);

  // Feed in SenderReport.
  const webrtc::NtpTime remote_ntp(0x9876543211);
  webrtc::rtcp::SenderReport sr;
  sr.SetSenderSsrc(kRemoteSsrc);
  sr.SetNtp(remote_ntp);
  auto raw_packet = sr.Build();
  rtcp_transceiver.ReceivePacket(raw_packet);

  int delay_ms = 100;
  clock.AdvanceTimeMilliseconds(delay_ms);

  // Trigger ReceiverReport back.
  RtcpPacketParser rtcp_parser;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillOnce(Invoke(&rtcp_parser, &RtcpPacketParser::Parse));
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  ASSERT_GT(rtcp_parser.receiver_report()->report_blocks().size(), 0u);
  const auto& report_block = rtcp_parser.receiver_report()->report_blocks()[0];
  EXPECT_EQ(report_block.source_ssrc(), kRemoteSsrc);
  EXPECT_EQ(report_block.last_sr(), webrtc::CompactNtp(remote_ntp));
  EXPECT_EQ(webrtc::CompactNtpRttToMs(report_block.delay_since_last_sr()),
            delay_ms);
}

}  // namespace
