/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/audio_decoder_factory_template.h"
#include "api/audio_codecs/audio_encoder_factory_template.h"
#include "api/audio_codecs/opus/audio_decoder_opus.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/test/loopback_media_transport.h"
#include "api/test/mock_audio_mixer.h"
#include "audio/audio_receive_stream.h"
#include "audio/audio_send_stream.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/task_queue.h"
#include "test/gtest.h"
#include "test/mock_transport.h"

namespace webrtc {
namespace test {

namespace {
const int kPayloadTypeOpus = 17;
}  // namespace

TEST(AudioWithMediaTransport, DeliversAudio) {
  MediaTransportPair transport_pair;
  MockTransport rtcp_send_transport;
  MockTransport send_transport;

  AudioState::Config audio_config;
  // TODO(nisse): Is a mock mixer enough?
  audio_config.audio_mixer = new rtc::RefCountedObject<MockAudioMixer>();
  audio_config.audio_processing =
      new rtc::RefCountedObject<MockAudioProcessing>();
  audio_config.audio_device_module =
      new rtc::RefCountedObject<testing::NiceMock<MockAudioDeviceModule>>();
  rtc::scoped_refptr<AudioState> audio_state = AudioState::Create(audio_config);

  // TODO(nisse): Use some lossless codec?
  const SdpAudioFormat audio_format("opus", 48000, 1);

  // Setup receive stream;
  webrtc::AudioReceiveStream::Config receive_config;
  receive_config.rtcp_send_transport = &rtcp_send_transport;
  receive_config.media_transport = transport_pair.first();
  receive_config.decoder_map.emplace(kPayloadTypeOpus, audio_format);
  receive_config.decoder_factory =
      CreateAudioDecoderFactory<AudioDecoderOpus>();

  std::unique_ptr<ProcessThread> receive_process_thread =
      ProcessThread::Create("audio recv thread");

  webrtc::internal::AudioReceiveStream receive_stream(
      /*rtp_stream_receiver_controller=*/nullptr,
      /*packet_router=*/nullptr, receive_process_thread.get(), receive_config,
      audio_state,
      /*event_log=*/nullptr);

  AudioSendStream::Config send_config(&send_transport, transport_pair.second());
  send_config.send_codec_spec =
      AudioSendStream::Config::SendCodecSpec(kPayloadTypeOpus, audio_format);
  send_config.encoder_factory = CreateAudioEncoderFactory<AudioEncoderOpus>();
  rtc::TaskQueue send_tq("audio send queue");
  std::unique_ptr<ProcessThread> send_process_thread =
      ProcessThread::Create("audio send thread");
  absl::optional<RtpState> rtp_state;
  TimeInterval life_time;
  webrtc::internal::AudioSendStream send_stream(
      send_config, audio_state, &send_tq, send_process_thread.get(),
      /*transport=*/nullptr,
      /*bitrate_allocator=*/nullptr,
      /*event_log=*/nullptr,
      /*rtcp_rtt_stats=*/nullptr, rtp_state, &life_time);

  receive_stream.Start();
  send_stream.Start();

  // TODO(nisse): Check audio delivery
  sleep(1);

  receive_stream.Stop();
  send_stream.Stop();
}

}  // namespace test
}  // namespace webrtc
