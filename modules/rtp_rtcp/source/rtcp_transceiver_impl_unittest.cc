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
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SizeIs;
using ::webrtc::MockTransport;
using ::webrtc::RtcpTransceiverConfig;
using ::webrtc::RtcpTransceiverImpl;
using ::webrtc::rtcp::ReportBlock;
using ::webrtc::test::RtcpPacketParser;

class MockReceiveStatisticsProvider : public webrtc::ReceiveStatisticsProvider {
 public:
  MOCK_METHOD1(RtcpReportBlocks, std::vector<ReportBlock>(size_t));
};

class FakeRtcpTransport : public webrtc::Transport {
 public:
  FakeRtcpTransport() : sent_rtcp_(false, false) {}
  bool SendRtcp(const uint8_t* data, size_t size) override {
    sent_rtcp_.Set();
    return true;
  }
  bool SendRtp(const uint8_t*, size_t, const webrtc::PacketOptions&) override {
    ADD_FAILURE() << "RtcpTransciver shouldn't send rtp packets.";
    return false;
  }

  bool WaitPacket(int64_t timeout_ms) { return sent_rtcp_.Wait(timeout_ms); }

 private:
  rtc::Event sent_rtcp_;
};

// Posting delayed tasks doesn't promise high precision.
constexpr int64_t kTaskQueuePrecisionMs = 15;

TEST(RtcpTransceiverImplTest, DelaySendingFirstCompondPacket) {
  rtc::TaskQueue queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.outgoing_transport = &transport;
  config.initial_delay_ms = 10;
  config.task_queue = &queue;
  std::unique_ptr<RtcpTransceiverImpl> rtcp_transceiver;

  int64_t started = rtc::TimeMillis();
  queue.PostTask(
      [&] { rtcp_transceiver = rtc::MakeUnique<RtcpTransceiverImpl>(config); });
  EXPECT_TRUE(
      transport.WaitPacket(config.initial_delay_ms + kTaskQueuePrecisionMs));

  EXPECT_GE(rtc::TimeMillis() - started, config.initial_delay_ms);

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
  config.initial_delay_ms = 0;
  config.min_periodic_report_ms = 10;
  config.task_queue = &queue;
  std::unique_ptr<RtcpTransceiverImpl> rtcp_transceiver;

  queue.PostTask(
      [&] { rtcp_transceiver = rtc::MakeUnique<RtcpTransceiverImpl>(config); });

  EXPECT_TRUE(
      transport.WaitPacket(config.initial_delay_ms + kTaskQueuePrecisionMs));
  int64_t time_of_1st_packet_ms = rtc::TimeMillis();
  EXPECT_TRUE(transport.WaitPacket(config.min_periodic_report_ms +
                                   kTaskQueuePrecisionMs));
  int64_t time_of_2nd_packet_ms = rtc::TimeMillis();

  EXPECT_GE(time_of_2nd_packet_ms - time_of_1st_packet_ms,
            config.min_periodic_report_ms);

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
  config.initial_delay_ms = 10;
  config.min_periodic_report_ms = 10;
  config.task_queue = &queue;
  std::unique_ptr<RtcpTransceiverImpl> rtcp_transceiver;
  queue.PostTask([&rtcp_transceiver, &config] {
    rtcp_transceiver = rtc::MakeUnique<RtcpTransceiverImpl>(config);
  });

  // Wait for first packet.
  EXPECT_TRUE(
      transport.WaitPacket(config.initial_delay_ms + kTaskQueuePrecisionMs));
  // Wait half-period time for next one - it shouldn't be sent.
  EXPECT_FALSE(transport.WaitPacket(config.min_periodic_report_ms / 2));
  // Send packet now.
  queue.PostTask([&] { rtcp_transceiver->SendCompoundPacket(); });
  EXPECT_TRUE(transport.WaitPacket(1));
  int64_t time_of_non_periodic_packet_ms = rtc::TimeMillis();
  // Next packet should be sent at least after period_ms.
  EXPECT_TRUE(transport.WaitPacket(config.min_periodic_report_ms +
                                   kTaskQueuePrecisionMs));
  EXPECT_GE(rtc::TimeMillis() - time_of_non_periodic_packet_ms,
            config.min_periodic_report_ms);

  // Cleanup.
  rtc::Event done(false, false);
  queue.PostTask([&] {
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(/*milliseconds=*/100));
}

TEST(RtcpTransceiverImplTest, SendCompoundPacketEmitsRtcpPacket) {
  MockTransport outgoing_transport;
  RtcpPacketParser rtcp_parser;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillOnce(Invoke(&rtcp_parser, &RtcpPacketParser::Parse));

  RtcpTransceiverConfig config;
  config.outgoing_transport = &outgoing_transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  ASSERT_EQ(rtcp_parser.receiver_report()->num_packets(), 0);
  rtcp_transceiver.SendCompoundPacket();
  EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
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

}  // namespace
