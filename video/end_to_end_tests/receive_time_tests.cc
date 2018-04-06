/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/timeutils.h"
#include "test/call_test.h"
#include "test/field_trial.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {
namespace {
constexpr int kRuntimeMs = 1700;
class ReportedReceiveTimeTester : public test::EndToEndTest {
 public:
  ReportedReceiveTimeTester()
      : EndToEndTest(test::CallTest::kDefaultTimeoutMs) {
    jumps_at_send_times_.push({2000, 500});
    jumps_at_send_times_.push({-400, 1000});
    jumps_at_send_times_.push({2000000, 1500});
  }
  bool JumpInReportedTimes() { return jump_in_reported_times_; }

 protected:
  Action OnReceiveRtcp(const uint8_t* data, size_t length) override {
    test::RtcpPacketParser parser;
    EXPECT_TRUE(parser.Parse(data, length));
    const auto& fb = parser.transport_feedback();
    if (fb->num_packets() > 0) {
      int64_t arrival_time_us = fb->GetBaseTimeUs();
      for (const auto& pkt : fb->GetReceivedPackets()) {
        arrival_time_us += pkt.delta_us();
        if (last_arrival_time_us_ != 0) {
          int64_t delta_us = arrival_time_us - last_arrival_time_us_;
          int64_t ground_truth_delta_us = send_times_us_[1] - send_times_us_[0];
          send_times_us_.pop_front();
          int64_t delta_diff_ms = (delta_us - ground_truth_delta_us) / 1000;
          if (std::abs(delta_diff_ms) > 200) {
            jump_in_reported_times_ = true;
            observation_complete_.Set();
          }
        }
        last_arrival_time_us_ = arrival_time_us;
      }
    }
    return SEND_PACKET;
  }
  Action OnSendRtp(const uint8_t* data, size_t length) override {
    send_times_us_.push_back(rtc::TimeMicros());
    int64_t now_ms = rtc::TimeMillis();
    if (!first_send_time_ms_)
      first_send_time_ms_ = now_ms;
    int64_t send_time_ms = now_ms - first_send_time_ms_;
    if (send_time_ms >= jumps_at_send_times_.front().second) {
      clock_offset_ms_ += jumps_at_send_times_.front().first;
      send_transport_->SetClockOffset(clock_offset_ms_);
      jumps_at_send_times_.pop();
    }
    return SEND_PACKET;
  }
  test::PacketTransport* CreateSendTransport(
      test::SingleThreadedTaskQueueForTesting* task_queue,
      Call* sender_call) override {
    return send_transport_ = new test::PacketTransport(
               task_queue, sender_call, this, test::PacketTransport::kSender,
               test::CallTest::payload_type_map_, FakeNetworkPipe::Config());
  }

  void PerformTest() override { observation_complete_.Wait(kRuntimeMs); }

  size_t GetNumVideoStreams() const override { return 1; }
  size_t GetNumAudioStreams() const override { return 0; }
  void OnRtpTransportControllerSendCreated(
      RtpTransportControllerSend* controller) override {
    controller_ = controller;
  }
  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {}

 private:
  RtpTransportControllerSend* controller_;
  int64_t last_arrival_time_us_ = 0;
  int64_t first_send_time_ms_ = 0;
  std::deque<int64_t> send_times_us_;
  bool jump_in_reported_times_ = false;
  test::PacketTransport* send_transport_;
  int64_t clock_offset_ms_ = 0;
  std::queue<std::pair<int64_t, int64_t>> jumps_at_send_times_;
};
}  // namespace

class ReceiveTimeEndToEndTest : public test::CallTest {
 public:
  ReceiveTimeEndToEndTest() {}

  virtual ~ReceiveTimeEndToEndTest() {}

 protected:
  void DecodesRetransmittedFrame(bool enable_rtx, bool enable_red);
  void ReceivesPliAndRecovers(int rtp_history_ms);
};

TEST_F(ReceiveTimeEndToEndTest, ReceiveTimeJumpsWithoutFieldTrial) {
  ReportedReceiveTimeTester test;
  RunBaseTest(&test);
  EXPECT_TRUE(test.JumpInReportedTimes());
}

TEST_F(ReceiveTimeEndToEndTest, ReceiveTimeSteadyWithFieldTrial) {
  test::ScopedFieldTrials field_trial(
      "WebRTC-BweReceiveTimeCorrection/Enabled,-100,1000/");
  ReportedReceiveTimeTester test;
  RunBaseTest(&test);
  EXPECT_FALSE(test.JumpInReportedTimes());
}
}  // namespace webrtc
