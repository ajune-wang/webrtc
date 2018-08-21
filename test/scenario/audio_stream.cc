/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/audio_stream.h"

#include "test/call_test.h"

namespace webrtc {
namespace test {

SendAudioStream::SendAudioStream(
    CallClient* sender,
    AudioStreamConfig config,
    rtc::scoped_refptr<AudioEncoderFactory> encoder_factory,
    Transport* send_transport)
    : sender_(sender), config_(config) {
  AudioSendStream::Config send_config(send_transport);
  send_config.rtp.ssrc = config.stream.ssrc.value_or(CallTest::kAudioSendSsrc);
  SdpAudioFormat::Parameters sdp_params;
  if (config.source.channels == 2)
    sdp_params["stereo"] = "1";
  if (config.encoder.frame_length != TimeDelta::ms(20))
    sdp_params["ptime"] = std::to_string(config.encoder.frame_length.ms());
  // SdpAudioFormat::num_channels indicates that the encoder is capable of
  // stereo, but the actual channel count used is based on the "stereo"
  // parameter.
  send_config.send_codec_spec = AudioSendStream::Config::SendCodecSpec(
      CallTest::kAudioSendPayloadType, {"opus", 48000, 2, sdp_params});
  RTC_DCHECK_LE(config.source.channels, 2);
  send_config.encoder_factory = encoder_factory;

  if (config.encoder.target_rate)
    send_config.send_codec_spec->target_bitrate_bps =
        config.encoder.target_rate->bps();

  if (config.encoder.min_rate)
    send_config.min_bitrate_bps = config.encoder.min_rate->bps();
  if (config.encoder.max_rate)
    send_config.max_bitrate_bps = config.encoder.max_rate->bps();

  if (config.stream.bitrate_tracking) {
    send_config.send_codec_spec->transport_cc_enabled = true;
    send_config.rtp.extensions = {
        {RtpExtension::kTransportSequenceNumberUri, 8}};
    if (!config.encoder.min_rate)
      send_config.min_bitrate_bps = config.encoder.target_rate->bps();
    if (!config.encoder.max_rate)
      send_config.max_bitrate_bps = config.encoder.target_rate->bps();
  }
  send_stream_ = sender_->call_->CreateAudioSendStream(send_config);
}

SendAudioStream::~SendAudioStream() {
  sender_->call_->DestroyAudioSendStream(send_stream_);
}

void SendAudioStream::Start() {
  send_stream_->Start();
}

bool SendAudioStream::TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                                       uint64_t receiver) {
  packet.SetSize(packet.size() - config_.stream.packet_overhead.bytes());
  sender_->DeliverPacket(MediaType::AUDIO, packet);
  return true;
}
ReceiveAudioStream::ReceiveAudioStream(
    CallClient* receiver,
    AudioStreamConfig config,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    Transport* feedback_transport)
    : receiver_(receiver), config_(config) {
  AudioReceiveStream::Config recv_config;
  recv_config.rtp.local_ssrc = CallTest::kReceiverLocalAudioSsrc;
  recv_config.rtcp_send_transport = feedback_transport;
  recv_config.rtp.remote_ssrc =
      config.stream.ssrc.value_or(CallTest::kAudioSendSsrc);
  if (config.stream.bitrate_tracking) {
    recv_config.rtp.transport_cc = true;
    recv_config.rtp.extensions = {
        {RtpExtension::kTransportSequenceNumberUri, 8}};
  }
  recv_config.decoder_factory = decoder_factory;
  recv_config.decoder_map = {
      {CallTest::kAudioSendPayloadType, {"opus", 48000, 2}}};
  recv_config.sync_group = config.render.sync_group;
  receive_stream_ = receiver_->call_->CreateAudioReceiveStream(recv_config);
}
ReceiveAudioStream::~ReceiveAudioStream() {
  receiver_->call_->DestroyAudioReceiveStream(receive_stream_);
}

bool ReceiveAudioStream::TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                                          uint64_t receiver) {
  packet.SetSize(packet.size() - config_.stream.packet_overhead.bytes());
  receiver_->DeliverPacket(MediaType::AUDIO, packet);
  return true;
}

}  // namespace test
}  // namespace webrtc
