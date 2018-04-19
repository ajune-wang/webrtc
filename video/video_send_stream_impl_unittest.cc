/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "call/test/mock_bitrate_allocator.h"
#include "call/test/mock_rtp_transport_controller_send.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/video_coding/fec_controller_default.h"
#include "rtc_base/experiments/alr_experiment.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "video/test/mock_video_stream_encoder.h"
#include "video/video_send_stream_impl.h"

namespace webrtc {
namespace internal {
namespace {
using testing::NiceMock;
using testing::StrictMock;
using testing::ReturnRef;
using testing::Return;
using testing::Invoke;
using testing::_;

constexpr int64_t kDefaultInitialBitrateBps = 333000;
const double kDefaultBitratePriority = 0.5;

const float kAlrProbingExperimentPaceMultiplier = 1.0f;
std::string GetAlrProbingExperimentString() {
  return std::string(
             AlrExperimentSettings::kScreenshareProbingBweExperimentName) +
         "/1.0,2875,80,40,-60,3/";
}

}  // namespace

class VideoSendStreamImplTest : public ::testing::Test {
 protected:
  VideoSendStreamImplTest()
      : clock(1000 * 1000 * 1000),
        config(&transport),
        send_delay_stats(&clock),
        retransmission_limiter(&clock, 1000),
        test_queue("test_queue"),
        process_thread(ProcessThread::Create("test_thread")),
        call_stats(&clock, process_thread.get()),
        stats_proxy(&clock,
                    config,
                    VideoEncoderConfig::ContentType::kRealtimeVideo) {
    config.rtp.ssrcs.push_back(8080);
    config.rtp.payload_type = 1;

    EXPECT_CALL(transport_controller, keepalive_config())
        .WillRepeatedly(ReturnRef(keepalive_config));
    EXPECT_CALL(transport_controller, packet_router())
        .WillRepeatedly(Return(&packet_router));
  }
  ~VideoSendStreamImplTest() {}

  std::unique_ptr<VideoSendStreamImpl> CreateVideoSendStreamImpl(
      int initial_encoder_max_bitrate,
      double initial_encoder_bitrate_priority,
      VideoEncoderConfig::ContentType content_type) {
    EXPECT_CALL(bitrate_allocator, GetStartBitrate(_)).WillOnce(Return(123000));
    std::map<uint32_t, RtpState> suspended_ssrcs;
    std::map<uint32_t, RtpPayloadState> suspended_payload_states;
    return rtc::MakeUnique<VideoSendStreamImpl>(
        &stats_proxy, &test_queue, &call_stats, &transport_controller,
        &bitrate_allocator, &send_delay_stats, &video_stream_encoder,
        &event_log, &config, initial_encoder_max_bitrate,
        initial_encoder_bitrate_priority, suspended_ssrcs,
        suspended_payload_states, content_type,
        rtc::MakeUnique<FecControllerDefault>(&clock), &retransmission_limiter);
  }

 protected:
  NiceMock<MockTransport> transport;
  NiceMock<MockRtpTransportControllerSend> transport_controller;
  NiceMock<MockBitrateAllocator> bitrate_allocator;
  NiceMock<MockVideoStreamEncoder> video_stream_encoder;

  SimulatedClock clock;
  RtcEventLogNullImpl event_log;
  VideoSendStream::Config config;
  SendDelayStats send_delay_stats;
  RateLimiter retransmission_limiter;
  rtc::test::TaskQueueForTest test_queue;
  std::unique_ptr<ProcessThread> process_thread;
  CallStats call_stats;
  SendStatisticsProxy stats_proxy;
  PacketRouter packet_router;
  RtpKeepAliveConfig keepalive_config;
};

TEST_F(VideoSendStreamImplTest, CanCreateVideoSendStreamImpl) {
  test_queue.SendTask([this] {
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, 0,
        VideoEncoderConfig::ContentType::kRealtimeVideo);
    vss_impl.reset();
  });
}

TEST_F(VideoSendStreamImplTest, RegistersAsBitrateObserverOnStart) {
  test_queue.SendTask([this] {
    config.track_id = "test";
    const bool kSuspend = false;
    config.suspend_below_min_bitrate = kSuspend;
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kRealtimeVideo);
    EXPECT_CALL(bitrate_allocator, AddObserver(vss_impl.get(), _))
        .WillOnce(Invoke(
            [&](BitrateAllocatorObserver*, MediaStreamAllocationConfig config) {
              EXPECT_EQ(config.min_bitrate_bps, 0u);
              EXPECT_EQ(config.max_bitrate_bps, kDefaultInitialBitrateBps);
              EXPECT_EQ(config.pad_up_bitrate_bps, 0u);
              EXPECT_EQ(config.enforce_min_bitrate, !kSuspend);
              EXPECT_EQ(config.track_id, "test");
              EXPECT_EQ(config.bitrate_priority, kDefaultBitratePriority);
              EXPECT_EQ(config.has_packet_feedback, false);
            }));
    vss_impl->Start();
    EXPECT_CALL(bitrate_allocator, RemoveObserver(vss_impl.get())).Times(1);
    vss_impl->Stop();
    vss_impl.reset();
  });
}

TEST_F(VideoSendStreamImplTest, ReportFeedbackAvailability) {
  test_queue.SendTask([this] {
    config.rtp.extensions.emplace_back(
        RtpExtension::kTransportSequenceNumberUri,
        RtpExtension::kTransportSequenceNumberDefaultId);
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kRealtimeVideo);
    EXPECT_CALL(bitrate_allocator, AddObserver(vss_impl.get(), _))
        .WillOnce(Invoke(
            [&](BitrateAllocatorObserver*, MediaStreamAllocationConfig config) {
              EXPECT_EQ(config.has_packet_feedback, true);
            }));
    vss_impl->Start();
    EXPECT_CALL(bitrate_allocator, RemoveObserver(vss_impl.get())).Times(1);
    vss_impl->Stop();
    vss_impl.reset();
  });
}

TEST_F(VideoSendStreamImplTest, SetsScreensharePacingFactorWithFeedback) {
  test::ScopedFieldTrials alr_experiment(GetAlrProbingExperimentString());

  test_queue.SendTask([this] {
    config.rtp.extensions.emplace_back(
        RtpExtension::kTransportSequenceNumberUri,
        RtpExtension::kTransportSequenceNumberDefaultId);
    EXPECT_CALL(transport_controller,
                SetPacingFactor(kAlrProbingExperimentPaceMultiplier))
        .Times(1);
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kScreen);
    vss_impl->Start();
    vss_impl->Stop();
    vss_impl.reset();
  });
}

TEST_F(VideoSendStreamImplTest, DoesNotSetPacingFactorWithoutFeedback) {
  test::ScopedFieldTrials alr_experiment(GetAlrProbingExperimentString());
  test_queue.SendTask([this] {
    EXPECT_CALL(transport_controller, SetPacingFactor(_)).Times(0);
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kScreen);
    vss_impl->Start();
    vss_impl->Stop();
    vss_impl.reset();
  });
}
}  // namespace internal
}  // namespace webrtc
