/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_ENGINE_H_
#define API_VOIP_VOIP_ENGINE_H_

#include <memory>

namespace webrtc {

class VoipBase;
class VoipCodec;
class VoipNetwork;

// VoipEngine interfaces
//
//   VoipEngineConfig config;
//   config.encoder_factory = CreateBuiltinAudioEncoderFactory();
//   config.decoder_factory = CreateBuiltinAudioDecoderFactory();
//   config.task_queue_factory = CreateDefaultTaskQueueFactory();
//
//   auto voip_engine = CreateVoipEngine(std::move(config));
//   if (!voip_engine) return some_failure;
//
//   auto& voip_base = voip_engine->Base();
//   auto& voip_codec = voip_engine->Codec();
//   auto& voip_network = voip_engine->Network();
//
//   absl::optional<ChannelId> channel =
//       voip_base.CreateChannel(&app_transport_);
//   if (!channel) return some_failure;
//
//   // After SDP offer/answer, set payload type and codecs that have been
//   // decided through SDP negotiation.
//   voip_codec.SetSendCodec(*channel, ...);
//   voip_codec.SetReceiveCodecs(*channel, ...);
//
//   // Start sending and playing RTP on voip channel.
//   voip_base.StartSend(*channel);
//   voip_base.StartPlayout(*channel);
//
//   // Inject received RTP/RTCP throught the VoipNetwork interface.
//   voip_network.ReceivedRTPPacket(*channel, ...);
//   voip_network.ReceivedRTCPPacket(*channel, ...);
//
//   // Stop and release voip channel.
//   voip_base.StopSend(*channel);
//   voip_base.StopPlayout(*channel);
//
//   voip_base.ReleaseChannel(*channel);
//
class VoipEngine {
 public:
  virtual ~VoipEngine() = default;

  // VoipBase is the audio session management interface that
  // create/release/start/stop one-to-one audio media session.
  virtual VoipBase& Base() = 0;

  // VoipNetwork provides injection APIs that would enable application
  // to send and receive RTP/RTCP packets. There is no default network module
  // that provides RTP transmission and reception.
  virtual VoipNetwork& Network() = 0;

  // VoipCodec provides codec configuration APIs for encoder and decoders.
  virtual VoipCodec& Codec() = 0;
};

}  // namespace webrtc

#endif  // API_VOIP_VOIP_ENGINE_H_
