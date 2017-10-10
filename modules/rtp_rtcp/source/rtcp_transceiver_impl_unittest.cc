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

TEST(RtcpTransceiverImplTest, PeriodicallySendsReceiverReport) {
  rtc::TaskQueue queue("rtcp");
  const uint32_t kSenderSsrc = 12345;
  const uint32_t kMediaSsrc = 54321;
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
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  config.outgoing_transport = &outgoing_transport;
  config.receive_statistics = &receive_statistics;
  config.min_periodic_report_ms = 10;

  std::unique_ptr<RtcpTransceiverImpl> rtcp_transceiver;

  queue.PostTask(
      [&] { rtcp_transceiver = rtc::MakeUnique<RtcpTransceiverImpl>(config); });
  EXPECT_FALSE(success.Wait(/*milliseconds=*/0));
  EXPECT_TRUE(success.Wait(/*milliseconds=*/25));
  // Cleanup.
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

  RtcpTransceiverConfig config;
  config.outgoing_transport = &outgoing_transport;
  config.schedule_periodic_reports = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.ForceSendReport();

  EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
}

}  // namespace
