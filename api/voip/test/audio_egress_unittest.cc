//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#include "api/voip/audio_egress.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/call/transport.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/voip/audio_ingress.h"
#include "modules/audio_mixer/sine_wave_generator.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "third_party/abseil-cpp/absl/synchronization/notification.h"

namespace webrtc {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Unused;

namespace {

constexpr uint64_t kStartTime = 123456789;
constexpr uint16_t kSeqNum = 12345;
constexpr uint32_t kRemoteSsrc = 0xDEADBEEF;
constexpr int16_t kAudioLevel = 3004;  // used for sine wave level
constexpr int kPcmuPayload = 0;
constexpr int kOpusPayload = 120;
const SdpAudioFormat kPcmuFormat = {"pcmu", 8000, 1};
const SdpAudioFormat kOpusFormat = {"opus", 48000, 2};

std::unique_ptr<RtpRtcp> CreateRtpStack(Clock* clock, Transport* transport) {
  RtpRtcp::Configuration rtp_config;
  rtp_config.clock = clock;
  rtp_config.audio = true;
  rtp_config.rtcp_report_interval_ms = 5000;
  rtp_config.outgoing_transport = transport;
  rtp_config.local_media_ssrc = kRemoteSsrc;
  auto rtp_rtcp = RtpRtcp::Create(rtp_config);
  rtp_rtcp->SetSendingMediaStatus(false);
  rtp_rtcp->SetRTCPStatus(RtcpMode::kCompound);
  return rtp_rtcp;
}

}  // namespace

class AudioEgressTest : public ::testing::Test {
 public:
  AudioEgressTest()
      : fake_clock_(kStartTime), wave_generator_(1000.0, kAudioLevel) {
    rtp_rtcp_ = CreateRtpStack(&fake_clock_, &transport_);
    task_queue_factory_ = CreateDefaultTaskQueueFactory();
    encoder_factory_ = CreateBuiltinAudioEncoderFactory();
  }

  void SetUp() override {
    egress_ = std::make_unique<AudioEgress>(rtp_rtcp_.get(), &fake_clock_,
                                            task_queue_factory_.get());
    egress_->SetEncoder(kPcmuPayload, kPcmuFormat,
                        encoder_factory_->MakeAudioEncoder(
                            kPcmuPayload, kPcmuFormat, absl::nullopt));
    egress_->Start();
    rtp_rtcp_->SetSequenceNumber(kSeqNum);
    rtp_rtcp_->SetSendingStatus(true);
  }

  void TearDown() override {
    rtp_rtcp_->SetSendingStatus(false);
    egress_->Stop();
    egress_.reset();
  }

  std::unique_ptr<AudioFrame> GetAudioFrame(uint32_t timestamp = 0,
                                            bool muted = false) {
    auto frame = std::make_unique<AudioFrame>();
    frame->sample_rate_hz_ = kPcmuFormat.clockrate_hz;
    frame->samples_per_channel_ = kPcmuFormat.clockrate_hz / 100;  // 10 ms.
    frame->num_channels_ = kPcmuFormat.num_channels;
    frame->timestamp_ = timestamp;
    if (muted) {
      frame->Mute();
    } else {
      wave_generator_.GenerateNextFrame(frame.get());
    }
    return frame;
  }

  void InsertPackets(size_t num_packets, bool muted = false) {
    // Two 10 ms audio frames will result in rtp packet with ptime 20.
    size_t required_frame_num = (num_packets * 2);
    for (size_t i = 0; i < required_frame_num; i++) {
      egress_->SendAudioData(
          GetAudioFrame(kPcmuFormat.clockrate_hz * i, muted));
      // advance 10 ms for each frame
      fake_clock_.AdvanceTimeMilliseconds(10);
    }
  }

  void ProcessMute(AudioFrame* frame, absl::Notification* notify) {
    egress_->encoder_queue_.PostTask([&] {
      egress_->ProcessMuteState(frame);
      notify->Notify();
    });
    notify->WaitForNotificationWithTimeout(absl::Milliseconds(10));
  }

  // SimulatedClock doesn't directly affect this testcase as the the
  // AudioFrame's timestamp is driven by GetAudioFrame.
  SimulatedClock fake_clock_;
  NiceMock<MockTransport> transport_;
  SineWaveGenerator wave_generator_;
  std::unique_ptr<AudioEgress> egress_;
  std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  std::unique_ptr<RtpRtcp> rtp_rtcp_;
  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory_;
};

TEST_F(AudioEgressTest, SendingStatusAfterStartAndStop) {
  EXPECT_TRUE(egress_->Sending());
  egress_->Stop();
  EXPECT_FALSE(egress_->Sending());
}

TEST_F(AudioEgressTest, ProcessAudioWithoutMute) {
  auto audio_frame = GetAudioFrame();
  AudioFrame copy_frame;
  copy_frame.CopyFrom(*audio_frame);

  absl::Notification notify;
  ProcessMute(audio_frame.get(), &notify);
  ASSERT_TRUE(notify.HasBeenNotified());

  size_t length =
      audio_frame->samples_per_channel_ * audio_frame->num_channels_;
  EXPECT_EQ(memcmp(audio_frame->data(), copy_frame.data(), length), 0);
}

TEST_F(AudioEgressTest, ProcessAudioAfterMute) {
  egress_->Mute(true);

  auto audio_frame = GetAudioFrame();
  AudioFrame copy_frame;
  copy_frame.CopyFrom(*audio_frame);

  absl::Notification notify;
  ProcessMute(audio_frame.get(), &notify);
  ASSERT_TRUE(notify.HasBeenNotified());

  size_t length =
      audio_frame->samples_per_channel_ * audio_frame->num_channels_;
  EXPECT_NE(memcmp(audio_frame->data(), copy_frame.data(), length), 0);
}

TEST_F(AudioEgressTest, ChangeEncoderFromPcmuToOpus) {
  EXPECT_EQ(egress_->EncoderSampleRate(), kPcmuFormat.clockrate_hz);
  EXPECT_EQ(egress_->EncoderNumChannel(), kPcmuFormat.num_channels);

  egress_->SetEncoder(kOpusPayload, kOpusFormat,
                      encoder_factory_->MakeAudioEncoder(
                          kOpusPayload, kOpusFormat, absl::nullopt));

  EXPECT_EQ(egress_->EncoderSampleRate(), kOpusFormat.clockrate_hz);
  EXPECT_EQ(egress_->EncoderNumChannel(), kOpusFormat.num_channels);
}

TEST_F(AudioEgressTest, SendDTMF) {
  constexpr int kExpected = 7;
  constexpr int kPayloadType = 100;
  constexpr int kDurationMs = 100;

  egress_->RegisterTelephoneEventType(kPayloadType,
                                      /*sample rate*/ 8000);
  // 100 ms duration will produce total 7 DTMF
  // 1 @ 20 ms, 2 @ 40 ms, 3 @ 60 ms, 4 @ 80 ms
  // 5, 6, 7 @ 100 ms (last one sends 3 dtmf)
  egress_->SendTelephoneEventOutband(/*event*/ 3, kDurationMs);

  absl::Notification notify;
  int counter = 0;
  auto rtp_sent = [&](const uint8_t* packet, size_t length, Unused) {
    RtpPacketReceived rtp_packet;
    rtp_packet.Parse(packet, length);

    bool is_dtmf = (rtp_packet.PayloadType() == kPayloadType &&
                    rtp_packet.SequenceNumber() == kSeqNum + counter &&
                    rtp_packet.padding_size() == 0 &&
                    rtp_packet.Marker() == (counter == 0) &&
                    rtp_packet.Ssrc() == kRemoteSsrc);

    if (is_dtmf && counter < kExpected && ++counter == kExpected) {
      notify.Notify();
    }
    return true;
  };

  EXPECT_CALL(transport_, SendRtp(_, _, _)).WillRepeatedly(Invoke(rtp_sent));

  InsertPackets(kExpected);
  notify.WaitForNotificationWithTimeout(absl::Milliseconds(100));
  ASSERT_TRUE(notify.HasBeenNotified());
}

}  // namespace webrtc
