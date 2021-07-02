/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/audio_receive_stream.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "api/test/mock_audio_mixer.h"
#include "api/test/mock_frame_decryptor.h"
#include "audio/conversion.h"
#include "call/rtp_stream_receiver_controller.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/audio_coding/neteq/default_neteq_factory.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/time_utils.h"
#include "test/gtest.h"
#include "test/mock_audio_decoder_factory.h"
#include "test/mock_transport.h"

namespace webrtc {
namespace test {
namespace {

using ::testing::_;
using ::testing::FloatEq;
using ::testing::NiceMock;
using ::testing::Return;

const uint32_t kRemoteSsrc = 1234;
const uint32_t kLocalSsrc = 5678;
const int kAudioLevelId = 3;
const int kTransportSequenceNumberId = 4;

struct ConfigHelper {
  explicit ConfigHelper(bool use_null_audio_processing)
      : ConfigHelper(rtc::make_ref_counted<MockAudioMixer>(),
                     use_null_audio_processing) {}

  ConfigHelper(rtc::scoped_refptr<MockAudioMixer> audio_mixer,
               bool use_null_audio_processing)
      : audio_mixer_(audio_mixer) {
    using ::testing::Invoke;

    AudioState::Config config;
    config.audio_mixer = audio_mixer_;
    config.audio_processing =
        use_null_audio_processing
            ? nullptr
            : rtc::make_ref_counted<NiceMock<MockAudioProcessing>>();
    config.audio_device_module =
        rtc::make_ref_counted<testing::NiceMock<MockAudioDeviceModule>>();
    audio_state_ = AudioState::Create(config);

    stream_config_.rtp.local_ssrc = kLocalSsrc;
    stream_config_.rtp.remote_ssrc = kRemoteSsrc;
    stream_config_.rtp.nack.rtp_history_ms = 300;
    stream_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId));
    stream_config_.rtp.extensions.push_back(RtpExtension(
        RtpExtension::kTransportSequenceNumberUri, kTransportSequenceNumberId));
    stream_config_.rtcp_send_transport = &rtcp_send_transport_;
    stream_config_.decoder_factory =
        rtc::make_ref_counted<MockAudioDecoderFactory>();
  }

  std::unique_ptr<internal::AudioReceiveStream> CreateAudioReceiveStream() {
    auto ret = std::make_unique<internal::AudioReceiveStream>(
        Clock::GetRealTimeClock(), &packet_router_, &neteq_factory_,
        stream_config_, audio_state_, &event_log_);
    ret->RegisterWithTransport(&rtp_stream_receiver_controller_);
    return ret;
  }

  AudioReceiveStream::Config& config() { return stream_config_; }
  rtc::scoped_refptr<MockAudioMixer> audio_mixer() { return audio_mixer_; }

 private:
  DefaultNetEqFactory neteq_factory_;
  PacketRouter packet_router_;
  MockRtcEventLog event_log_;
  rtc::scoped_refptr<AudioState> audio_state_;
  rtc::scoped_refptr<MockAudioMixer> audio_mixer_;
  AudioReceiveStream::Config stream_config_;
  RtpStreamReceiverController rtp_stream_receiver_controller_;
  MockTransport rtcp_send_transport_;
};

}  // namespace

TEST(AudioReceiveStreamTest, ConfigToString) {
  AudioReceiveStream::Config config;
  config.rtp.remote_ssrc = kRemoteSsrc;
  config.rtp.local_ssrc = kLocalSsrc;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId));
  EXPECT_EQ(
      "{rtp: {remote_ssrc: 1234, local_ssrc: 5678, transport_cc: off, nack: "
      "{rtp_history_ms: 0}, extensions: [{uri: "
      "urn:ietf:params:rtp-hdrext:ssrc-audio-level, id: 3}]}, "
      "rtcp_send_transport: null}",
      config.ToString());
}

TEST(AudioReceiveStreamTest, ConstructDestruct) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(use_null_audio_processing);
    auto recv_stream = helper.CreateAudioReceiveStream();
    recv_stream->UnregisterFromTransport();
  }
}

TEST(AudioReceiveStreamTest, StreamsShouldBeAddedToMixerOnceOnStart) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper1(use_null_audio_processing);
    ConfigHelper helper2(helper1.audio_mixer(), use_null_audio_processing);
    auto recv_stream1 = helper1.CreateAudioReceiveStream();
    auto recv_stream2 = helper2.CreateAudioReceiveStream();

    EXPECT_CALL(*helper1.audio_mixer(), AddSource(recv_stream1.get()))
        .WillOnce(Return(true));
    EXPECT_CALL(*helper1.audio_mixer(), AddSource(recv_stream2.get()))
        .WillOnce(Return(true));
    EXPECT_CALL(*helper1.audio_mixer(), RemoveSource(recv_stream1.get()))
        .Times(1);
    EXPECT_CALL(*helper1.audio_mixer(), RemoveSource(recv_stream2.get()))
        .Times(1);

    recv_stream1->Start();
    recv_stream2->Start();

    // One more should not result in any more mixer sources added.
    recv_stream1->Start();

    // Stop stream before it is being destructed.
    recv_stream2->Stop();

    recv_stream1->UnregisterFromTransport();
    recv_stream2->UnregisterFromTransport();
  }
}

TEST(AudioReceiveStreamTest, ReconfigureWithFrameDecryptor) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(use_null_audio_processing);
    auto recv_stream = helper.CreateAudioReceiveStream();

    auto new_config_0 = helper.config();
    rtc::scoped_refptr<FrameDecryptorInterface> mock_frame_decryptor_0(
        rtc::make_ref_counted<MockFrameDecryptor>());
    new_config_0.frame_decryptor = mock_frame_decryptor_0;

    // TODO(tommi): While this changes the internal config value, it doesn't
    // actually change what frame_decryptor is used.  WebRtcAudioReceiveStream
    // recreates the whole instance in order to change this value.
    // So, it's not clear if changing this post initialization needs to be
    // supported.
    recv_stream->ReconfigureForTesting(new_config_0);

    auto new_config_1 = helper.config();
    rtc::scoped_refptr<FrameDecryptorInterface> mock_frame_decryptor_1(
        rtc::make_ref_counted<MockFrameDecryptor>());
    new_config_1.frame_decryptor = mock_frame_decryptor_1;
    new_config_1.crypto_options.sframe.require_frame_encryption = true;
    recv_stream->ReconfigureForTesting(new_config_1);
    recv_stream->UnregisterFromTransport();
  }
}

}  // namespace test
}  // namespace webrtc
