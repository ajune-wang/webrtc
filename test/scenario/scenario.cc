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

#include <algorithm>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"

namespace webrtc {
namespace test {
namespace {
void Route(int64_t receiver_id,
           NetworkReceiverInterface* receiver,
           std::vector<NetworkNode*> nodes) {
  RTC_CHECK(!nodes.empty());
  for (size_t i = 0; i + 1 < nodes.size(); ++i)
    nodes[i]->SetRoute(receiver_id, nodes[i + 1]);
  nodes.back()->SetRoute(receiver_id, receiver);
}
}  // namespace

RepeatedActivity::RepeatedActivity(TimeDelta interval,
                                   std::function<void(TimeDelta)> function)
    : interval_(interval), function_(function) {}

void RepeatedActivity::Stop() {
  interval_ = TimeDelta::PlusInfinity();
}

void RepeatedActivity::Poll(Timestamp time) {
  RTC_DCHECK(last_update_.IsFinite());
  if (time >= last_update_ + interval_) {
    function_(time - last_update_);
    last_update_ = time;
  }
}

void RepeatedActivity::SetStartTime(Timestamp time) {
  last_update_ = time;
}

Timestamp RepeatedActivity::NextTime() {
  RTC_DCHECK(last_update_.IsFinite());
  return last_update_ + interval_;
}

Scenario::Scenario() : Scenario("") {}

Scenario::Scenario(std::string log_path)
    : base_filename_(log_path),
      audio_decoder_factory_(CreateBuiltinAudioDecoderFactory()),
      audio_encoder_factory_(CreateBuiltinAudioEncoderFactory()) {}

ColumnPrinter Scenario::TimePrinter() {
  return ColumnPrinter::Lambda("time",
                               [this](rtc::SimpleStringBuilder& sb) {
                                 sb.AppendFormat("%.3lf",
                                                 Now().seconds<double>());
                               },
                               32);
}

CallClient* Scenario::CreateClient(std::string name, CallClientConfig config) {
  CallClient* client =
      new CallClient(Clock::GetRealTimeClock(), name, config, base_filename_);
  if (!base_filename_.empty() && !name.empty() &&
      config.cc.log_interval.IsFinite()) {
    Every(config.cc.log_interval,
          [client]() { client->LogCongestionControllerStats(); });
  }
  clients_.emplace_back(client);
  return client;
}

CallClient* Scenario::CreateClient(
    std::string name,
    std::function<void(CallClientConfig*)> config_modifier) {
  CallClientConfig config;
  config_modifier(&config);
  return CreateClient(name, config);
}

SimulationNode* Scenario::CreateNetworkNode(
    std::function<void(NetworkNodeConfig*)> config_modifier) {
  NetworkNodeConfig config;
  config_modifier(&config);
  return CreateNetworkNode(config);
}

SimulationNode* Scenario::CreateNetworkNode(NetworkNodeConfig config) {
  auto network_node = SimulationNode::Create(Clock::GetRealTimeClock(), config);
  SimulationNode* network_node_ptr = network_node.get();
  network_nodes_.emplace_back(std::move(network_node));
  Every(config.update_frequency,
        [network_node_ptr] { network_node_ptr->Process(); });
  return network_node_ptr;
}

NetworkNode* Scenario::CreateNetworkNode(
    NetworkNodeConfig config,
    std::unique_ptr<NetworkSimulationInterface> simulation) {
  network_nodes_.emplace_back(new NetworkNode(Clock::GetRealTimeClock(), config,
                                              std::move(simulation)));
  return network_nodes_.back().get();
}

CrossTrafficSource* Scenario::CreateCrossTraffic(
    std::vector<NetworkNode*> over_nodes,
    std::function<void(CrossTrafficConfig*)> config_modifier) {
  CrossTrafficConfig cross_config;
  config_modifier(&cross_config);
  return CreateCrossTraffic(over_nodes, cross_config);
}

CrossTrafficSource* Scenario::CreateCrossTraffic(
    std::vector<NetworkNode*> over_nodes,
    CrossTrafficConfig config) {
  int64_t receiver_id = next_receiver_id_++;
  cross_traffic_sources_.emplace_back(
      new CrossTrafficSource(over_nodes.front(), receiver_id, config));
  CrossTrafficSource* node = cross_traffic_sources_.back().get();
  Route(receiver_id, &null_receiver_, over_nodes);
  Every(config.min_packet_interval,
        [node](TimeDelta delta) { node->Process(delta); });
  return node;
}

std::pair<SendVideoStream*, ReceiveVideoStream*> Scenario::CreateVideoStreams(
    CallClient* sender,
    std::vector<NetworkNode*> send_link,
    CallClient* receiver,
    std::vector<NetworkNode*> return_link,
    std::function<void(VideoStreamConfig*)> config_modifier) {
  VideoStreamConfig config;
  config_modifier(&config);
  return CreateVideoStreams(sender, send_link, receiver, return_link, config);
}

std::pair<SendVideoStream*, ReceiveVideoStream*> Scenario::CreateVideoStreams(
    CallClient* sender,
    std::vector<NetworkNode*> send_link,
    CallClient* receiver,
    std::vector<NetworkNode*> return_link,
    VideoStreamConfig config) {
  for (size_t i = config.stream.ssrcs.size();
       i < config.encoder.num_simulcast_streams; ++i)
    config.stream.ssrcs.push_back(sender->GetNextVideoSsrc());
  config.stream.ssrcs.resize(config.encoder.num_simulcast_streams);

  for (size_t i = config.stream.rtx_ssrcs.size();
       i < config.stream.num_rtx_streams; ++i)
    config.stream.rtx_ssrcs.push_back(sender->GetNextRtxSsrc());
  config.stream.rtx_ssrcs.resize(config.stream.num_rtx_streams);

  std::unique_ptr<NetworkNodeTransport> send_transport(
      new NetworkNodeTransport(sender, send_link.front(), next_receiver_id_++,
                               config.stream.packet_overhead));

  std::unique_ptr<SendVideoStream> send_stream(
      new SendVideoStream(sender, config, send_transport.get()));

  std::unique_ptr<NetworkNodeTransport> return_transport(
      new NetworkNodeTransport(receiver, return_link.front(),
                               next_receiver_id_++,
                               config.stream.packet_overhead));

  size_t chosen_stream = 0;
  std::unique_ptr<ReceiveVideoStream> recv_stream(new ReceiveVideoStream(
      receiver, config, chosen_stream, return_transport.get()));

  Route(send_transport->ReceiverId(), recv_stream.get(), send_link);
  Route(return_transport->ReceiverId(), send_stream.get(), return_link);

  transports_.emplace_back(std::move(send_transport));
  transports_.emplace_back(std::move(return_transport));
  send_video_streams_.emplace_back(std::move(send_stream));
  receive_video_streams_.emplace_back(std::move(recv_stream));

  return {send_video_streams_.back().get(),
          receive_video_streams_.back().get()};
}

std::pair<SendAudioStream*, ReceiveAudioStream*> Scenario::CreateAudioStreams(
    CallClient* sender,
    std::vector<NetworkNode*> send_link,
    CallClient* receiver,
    std::vector<NetworkNode*> return_link,
    std::function<void(AudioStreamConfig*)> config_modifier) {
  AudioStreamConfig config;
  config_modifier(&config);
  return CreateAudioStreams(sender, send_link, receiver, return_link, config);
}

std::pair<SendAudioStream*, ReceiveAudioStream*> Scenario::CreateAudioStreams(
    CallClient* sender,
    std::vector<NetworkNode*> send_link,
    CallClient* receiver,
    std::vector<NetworkNode*> return_link,
    AudioStreamConfig config) {
  std::unique_ptr<NetworkNodeTransport> send_transport(
      new NetworkNodeTransport(sender, send_link.front(), next_receiver_id_++,
                               config.stream.packet_overhead));

  std::unique_ptr<SendAudioStream> send_stream(new SendAudioStream(
      sender, config, audio_encoder_factory_, send_transport.get()));

  std::unique_ptr<NetworkNodeTransport> return_transport(
      new NetworkNodeTransport(receiver, return_link.front(),
                               next_receiver_id_++,
                               config.stream.packet_overhead));

  std::unique_ptr<ReceiveAudioStream> recv_stream(new ReceiveAudioStream(
      receiver, config, audio_decoder_factory_, return_transport.get()));

  Route(send_transport->ReceiverId(), recv_stream.get(), send_link);
  Route(return_transport->ReceiverId(), send_stream.get(), return_link);

  transports_.emplace_back(std::move(send_transport));
  transports_.emplace_back(std::move(return_transport));
  send_audio_streams_.emplace_back(std::move(send_stream));
  receive_audio_streams_.emplace_back(std::move(recv_stream));

  return {send_audio_streams_.back().get(),
          receive_audio_streams_.back().get()};
}

RepeatedActivity* Scenario::Every(TimeDelta interval,
                                  std::function<void(TimeDelta)> function) {
  repeated_activities_.emplace_back(new RepeatedActivity(interval, function));
  return repeated_activities_.back().get();
}

RepeatedActivity* Scenario::Every(TimeDelta interval,
                                  std::function<void()> function) {
  auto function_with_argument = [function](TimeDelta) { function(); };
  repeated_activities_.emplace_back(
      new RepeatedActivity(interval, function_with_argument));
  return repeated_activities_.back().get();
}

void Scenario::RunFor(TimeDelta duration) {
  RunUntil(duration, TimeDelta::PlusInfinity(), []() { return false; });
}

void Scenario::RunUntil(TimeDelta max_duration,
                        TimeDelta poll_interval,
                        std::function<bool()> exit_function) {
  start_time_ = Timestamp::us(rtc::TimeMicros());
  for (auto& activity : repeated_activities_) {
    activity->SetStartTime(start_time_);
  }

  for (auto& stream : receive_video_streams_)
    stream->receive_stream_->Start();
  for (auto& stream : receive_audio_streams_)
    stream->receive_stream_->Start();
  for (auto& stream : send_video_streams_) {
    stream->send_stream_->Start();
    stream->video_capturer_->Start();
  }
  for (auto& stream : send_audio_streams_)
    stream->send_stream_->Start();
  for (auto& call : clients_) {
    call->call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
    call->call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
  }

  rtc::Event done_(false, false);
  while (!exit_function() && Duration() < max_duration) {
    Timestamp current_time = Timestamp::us(rtc::TimeMicros());
    Timestamp next_time = poll_interval.IsInfinite()
                              ? Timestamp::Infinity()
                              : current_time + poll_interval;
    for (auto& activity : repeated_activities_) {
      activity->Poll(current_time);
      next_time = std::min(next_time, activity->NextTime());
    }
    TimeDelta wait_time = next_time - current_time;
    done_.Wait(wait_time.ms<int>());
  }
  for (auto& stream : send_video_streams_) {
    stream->video_capturer_->Stop();
    stream->send_stream_->Stop();
  }
  for (auto& stream : send_audio_streams_)
    stream->send_stream_->Stop();
  for (auto& stream : receive_video_streams_)
    stream->receive_stream_->Stop();
  for (auto& stream : receive_audio_streams_)
    stream->receive_stream_->Stop();
}

Timestamp Scenario::Now() {
  return Timestamp::us(rtc::TimeMicros());
}

TimeDelta Scenario::Duration() {
  return Now() - start_time_;
}

}  // namespace test
}  // namespace webrtc
