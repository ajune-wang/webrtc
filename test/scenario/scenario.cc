/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/scenario.h"

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"

namespace webrtc {
namespace test {

TimeTrigger::TimeTrigger(TimeDelta relative_time)
    : relative_time_(relative_time) {}

Scenario::Scenario() : Scenario("") {}

Scenario::Scenario(std::string log_path)
    : base_filename_(log_path),
      audio_decoder_factory_(CreateBuiltinAudioDecoderFactory()),
      audio_encoder_factory_(CreateBuiltinAudioEncoderFactory()) {}

LambdaPrinter Scenario::TimePrinter() {
  return {"time",
          [this](rtc::SimpleStringBuilder& sb) {
            sb.AppendFormat("%.3lf", Now().seconds<double>());
          },
          32};
}

std::pair<SendVideoStream*, ReceiveVideoStream*> Scenario::CreateVideoStreams(
    CallClient* sender,
    NetworkNode* send_link,
    CallClient* receiver,
    NetworkNode* return_link,
    VideoStreamConfig config) {
  int64_t receive_id = next_receiver_id_++;
  int64_t feedback_id = next_receiver_id_++;

  for (size_t i = config.stream.ssrcs.size();
       i < config.encoder.num_simulcast_streams; ++i)
    config.stream.ssrcs.push_back(sender->GetNextVideoSsrc());
  config.stream.ssrcs.resize(config.encoder.num_simulcast_streams);

  for (size_t i = config.stream.rtx_ssrcs.size();
       i < config.stream.num_rtx_streams; ++i)
    config.stream.rtx_ssrcs.push_back(sender->GetNextRtxSsrc());
  config.stream.rtx_ssrcs.resize(config.stream.num_rtx_streams);

  auto send_transport =
      absl::make_unique<NetworkNodeTransport>(send_link, receive_id);
  auto send_stream =
      absl::make_unique<SendVideoStream>(sender, config, send_transport.get());

  auto return_transport =
      absl::make_unique<NetworkNodeTransport>(return_link, feedback_id);
  auto recv_stream = absl::make_unique<ReceiveVideoStream>(
      receiver, config, return_transport.get());

  send_link->SetRoute(receive_id, recv_stream.get());
  return_link->SetRoute(feedback_id, send_stream.get());

  transports_.emplace_back(std::move(send_transport));
  transports_.emplace_back(std::move(return_transport));
  send_video_streams_.emplace_back(std::move(send_stream));
  receive_video_streams_.emplace_back(std::move(recv_stream));

  return {send_video_streams_.back().get(),
          receive_video_streams_.back().get()};
}

std::pair<SendAudioStream*, ReceiveAudioStream*> Scenario::CreateAudioStreams(
    CallClient* sender,
    NetworkNode* send_link,
    CallClient* receiver,
    NetworkNode* return_link,
    AudioStreamConfig config) {
  int64_t receive_id = next_receiver_id_++;
  int64_t feedback_id = next_receiver_id_++;

  auto send_transport =
      absl::make_unique<NetworkNodeTransport>(send_link, receive_id);
  auto send_stream = absl::make_unique<SendAudioStream>(
      sender, config, audio_encoder_factory_, send_transport.get());

  auto return_transport =
      absl::make_unique<NetworkNodeTransport>(return_link, feedback_id);
  auto recv_stream = absl::make_unique<ReceiveAudioStream>(
      receiver, config, audio_decoder_factory_, return_transport.get());

  send_link->SetRoute(receive_id, recv_stream.get());
  return_link->SetRoute(feedback_id, send_stream.get());

  transports_.emplace_back(std::move(send_transport));
  transports_.emplace_back(std::move(return_transport));
  send_audio_streams_.emplace_back(std::move(send_stream));
  receive_audio_streams_.emplace_back(std::move(recv_stream));

  return {send_audio_streams_.back().get(),
          receive_audio_streams_.back().get()};
}

}  // namespace test
}  // namespace webrtc
