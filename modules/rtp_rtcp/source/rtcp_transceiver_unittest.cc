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

#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "rtc_base/event.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/task_queue.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"

namespace {

using ::testing::Invoke;
using ::testing::_;
using ::webrtc::MockTransport;
using ::webrtc::RtcpTransceiver;
using ::webrtc::rtcp::SenderReport;

void WaitPostedTasks(rtc::TaskQueue* queue) {
  rtc::Event done(false, false);
  queue->PostTask([&done] { done.Set(); });
  ASSERT_TRUE(done.Wait(100));
}

TEST(RtcpTransceiverTest, SendsRtcpOnTaskQueueWhenCreatedOffTaskQueue) {
  MockTransport outgoing_transport;
  RtcpTransceiver::Configuration config;
  config.min_periodic_report_ms = 10;
  config.outgoing_transport = &outgoing_transport;
  rtc::Event success(false, false);
  rtc::TaskQueue queue("rtcp");

  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillRepeatedly(Invoke([&](const uint8_t*, size_t) {
        EXPECT_TRUE(queue.IsCurrent());
        success.Set();
        return true;
      }));

  auto rtcp_transceiver = rtc::MakeUnique<RtcpTransceiver>(&queue, config);
  EXPECT_TRUE(success.Wait(199));
  rtcp_transceiver.reset();
  WaitPostedTasks(&queue);
}

TEST(RtcpTransceiverTest, SendsRtcpOnTaskQueueWhenCreatedOnTaskQueue) {
  MockTransport outgoing_transport;
  RtcpTransceiver::Configuration config;
  config.min_periodic_report_ms = 10;
  config.outgoing_transport = &outgoing_transport;
  rtc::Event success(false, false);
  rtc::TaskQueue queue("rtcp");

  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillRepeatedly(Invoke([&](const uint8_t*, size_t) {
        EXPECT_TRUE(queue.IsCurrent());
        success.Set();
        return true;
      }));

  std::unique_ptr<RtcpTransceiver> rtcp_transceiver;
  queue.PostTask([&] {
    rtcp_transceiver = rtc::MakeUnique<RtcpTransceiver>(&queue, config);
  });
  EXPECT_TRUE(success.Wait(199));
  queue.PostTask([&] {
    EXPECT_TRUE(rtcp_transceiver);
    rtcp_transceiver.reset();
  });
  WaitPostedTasks(&queue);
  EXPECT_FALSE(rtcp_transceiver);
}

TEST(RtcpTransceiverTest, ThreadSafe) {
  MockTransport outgoing_transport;
  RtcpTransceiver::Configuration config;
  config.min_periodic_report_ms = 10;
  config.outgoing_transport = &outgoing_transport;
  rtc::Event start(/*manual=*/true, false);
  rtc::TaskQueue queue("rtcp");

  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillRepeatedly(Invoke([&](const uint8_t*, size_t) {
        EXPECT_TRUE(queue.IsCurrent());
        return true;
      }));

  RtcpTransceiver rtcp_transceiver(&queue, config);
  auto sr_raw = SenderReport().Build();
  rtc::CopyOnWriteBuffer raw_packet(sr_raw.data(), sr_raw.size());

  rtc::TaskQueue queue1("receive_packet");
  rtc::TaskQueue queue2("send_packet");
  queue1.PostTask([&] {
    start.Wait(rtc::Event::kForever);
    rtcp_transceiver.ReceivePacket(raw_packet);
  });
  queue2.PostTask([&] {
    start.Wait(rtc::Event::kForever);
    rtcp_transceiver.ForceSendReport();
  });

  start.Set();

  WaitPostedTasks(&queue1);
  WaitPostedTasks(&queue2);
  WaitPostedTasks(&queue);
}

TEST(RtcpTransceiverTest, DoesntSendPacketsAfterDestruction) {
  MockTransport outgoing_transport;
  RtcpTransceiver::Configuration config;
  config.schedule_periodic_reports = false;
  config.outgoing_transport = &outgoing_transport;
  rtc::TaskQueue queue("rtcp");

  EXPECT_CALL(outgoing_transport, SendRtcp(_, _)).Times(0);

  auto rtcp_transceiver = rtc::MakeUnique<RtcpTransceiver>(&queue, config);

  rtc::Event pause(false, false);
  queue.PostTask([&] {
    pause.Wait(rtc::Event::kForever);
    rtcp_transceiver.reset();
  });
  rtcp_transceiver->ForceSendReport();
  pause.Set();
  WaitPostedTasks(&queue);
  EXPECT_FALSE(rtcp_transceiver);
}

}  // namespace
