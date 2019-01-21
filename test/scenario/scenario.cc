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

#include "absl/memory/memory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "rtc_base/flags.h"
#include "rtc_base/socket_address.h"
#include "test/logging/file_log_writer.h"
#include "test/scenario/network/cross_traffic.h"
#include "test/scenario/network/network_emulation.h"
#include "test/testsupport/file_utils.h"

WEBRTC_DEFINE_bool(scenario_logs, false, "Save logs from scenario framework.");
WEBRTC_DEFINE_string(out_root,
                     "",
                     "Output root path, based on project root if unset.");

namespace webrtc {
namespace test {
namespace {
int64_t kMicrosPerSec = 1000000;
std::unique_ptr<FileLogWriterFactory> GetScenarioLogManager(
    std::string file_name) {
  if (FLAG_scenario_logs && !file_name.empty()) {
    std::string output_root = FLAG_out_root;
    if (output_root.empty())
      output_root = OutputPath() + "output_data/";

    auto base_filename = output_root + file_name + ".";
    RTC_LOG(LS_INFO) << "Saving scenario logs to: " << base_filename;
    return absl::make_unique<FileLogWriterFactory>(base_filename);
  }
  return nullptr;
}
}

RepeatedActivity::RepeatedActivity(TimeDelta interval,
                                   std::function<void(TimeDelta)> function)
    : interval_(interval), function_(function) {}

void RepeatedActivity::Stop() {
  interval_ = TimeDelta::PlusInfinity();
}

void RepeatedActivity::Execute(Timestamp time) {
  if (last_update_.IsInfinite()) {
    last_update_ = time;
  }
  if (time >= last_update_ + interval_) {
    function_(time - last_update_);
    last_update_ = time;
  }
}

TimeDelta RepeatedActivity::TimeToNextExecution() const {
  return interval_;
}

void RepeatedActivity::SetStartTime(Timestamp time) {
  last_update_ = time;
}

Timestamp RepeatedActivity::NextTime() {
  RTC_DCHECK(last_update_.IsFinite());
  return last_update_ + interval_;
}

Scenario::Scenario()
    : Scenario(std::unique_ptr<LogWriterFactoryInterface>(), true) {}

Scenario::Scenario(std::string file_name) : Scenario(file_name, true) {}

Scenario::Scenario(std::string file_name, bool real_time)
    : Scenario(GetScenarioLogManager(file_name), real_time) {}

Scenario::Scenario(
    std::unique_ptr<LogWriterFactoryInterface> log_writer_factory,
    bool real_time)
    : log_writer_factory_(std::move(log_writer_factory)),
      real_time_mode_(real_time),
      sim_clock_(100000 * kMicrosPerSec),
      clock_(real_time ? Clock::GetRealTimeClock() : &sim_clock_),
      audio_decoder_factory_(CreateBuiltinAudioDecoderFactory()),
      audio_encoder_factory_(CreateBuiltinAudioEncoderFactory()) {
  if (real_time)
    time_controller_ = absl::make_unique<RealTimeController>();
  else {
    auto controller = absl::make_unique<SimulatedTimeController>(&sim_clock_);
    controller->SetGlobalFakeClock(&event_log_fake_clock_);
    time_controller_ = std::move(controller);
  }
  network_emulation_manager_ =
      absl::make_unique<NetworkEmulationManager>(time_controller_.get());
  if (!real_time_mode_ && log_writer_factory_) {
    rtc::SetClockForTesting(&event_log_fake_clock_);
    event_log_fake_clock_.SetTimeNanos(sim_clock_.TimeInMicroseconds() * 1000);
  }
}

Scenario::~Scenario() {
  if (start_time_.IsFinite())
    Stop();
  if (!real_time_mode_)
    rtc::SetClockForTesting(nullptr);
  audio_streams_.clear();
  video_streams_.clear();
  client_pairs_.clear();
  for (size_t i = 0; i < clients_.size(); ++i) {
    clients_[i]->thread()->Invoke<void>(RTC_FROM_HERE,
                                        [&]() { clients_[i].reset(nullptr); });
  }
}

ColumnPrinter Scenario::TimePrinter() {
  return ColumnPrinter::Lambda("time",
                               [this](rtc::SimpleStringBuilder& sb) {
                                 sb.AppendFormat("%.3lf",
                                                 Now().seconds<double>());
                               },
                               32);
}

StatesPrinter* Scenario::CreatePrinter(std::string name,
                                       TimeDelta interval,
                                       std::vector<ColumnPrinter> printers) {
  std::vector<ColumnPrinter> all_printers{TimePrinter()};
  for (auto& printer : printers)
    all_printers.push_back(printer);
  StatesPrinter* printer = new StatesPrinter(GetLogWriter(name), all_printers);
  printers_.emplace_back(printer);
  printer->PrintHeaders();
  if (interval.IsFinite())
    Every(interval, [printer] { printer->PrintRow(); });
  return printer;
}

CallClient* Scenario::CreateClient(std::string name, CallClientConfig config) {
  RTC_DCHECK(real_time_mode_);

  EndpointNode* endpoint = network_emulation_manager_->CreateEndpoint(
      rtc::IPAddress(next_route_id_++));
  rtc::Thread* network_thread =
      network_emulation_manager_->CreateNetworkThread({endpoint});
  CallClient* client =
      network_thread->Invoke<CallClient*>(RTC_FROM_HERE, [&]() {
        return new CallClient(clock_, GetLogWriterFactory(name), config,
                              endpoint, network_thread);
      });

  if (config.transport.state_log_interval.IsFinite()) {
    Every(config.transport.state_log_interval, [this, client]() {
      client->network_controller_factory_.LogCongestionControllerStats(Now());
    });
  }
  clients_.emplace_back(client);

  RTC_LOG(INFO) << "Created client with endpoint on IP "
                << endpoint->GetPeerLocalAddress().ToString();
  return client;
}

CallClient* Scenario::CreateClient(
    std::string name,
    std::function<void(CallClientConfig*)> config_modifier) {
  CallClientConfig config;
  config_modifier(&config);
  return CreateClient(name, config);
}

CallClientPair* Scenario::CreateRoutes(
    CallClient* first,
    std::vector<EmulatedNetworkNode*> send_link,
    CallClient* second,
    std::vector<EmulatedNetworkNode*> return_link) {
  return CreateRoutes(first, send_link,
                      DataSize::bytes(PacketOverhead::kDefault), second,
                      return_link, DataSize::bytes(PacketOverhead::kDefault));
}

CallClientPair* Scenario::CreateRoutes(
    CallClient* first,
    std::vector<EmulatedNetworkNode*> send_link,
    DataSize first_overhead,
    CallClient* second,
    std::vector<EmulatedNetworkNode*> return_link,
    DataSize second_overhead) {
  CallClientPair* client_pair = new CallClientPair(first, second);
  ChangeRoute(client_pair->forward(), send_link, first_overhead);
  ChangeRoute(client_pair->reverse(), return_link, second_overhead);
  client_pairs_.emplace_back(client_pair);
  return client_pair;
}

void Scenario::ChangeRoute(std::pair<CallClient*, CallClient*> clients,
                           std::vector<EmulatedNetworkNode*> over_nodes) {
  ChangeRoute(clients, over_nodes, DataSize::bytes(PacketOverhead::kDefault));
}

void Scenario::ChangeRoute(std::pair<CallClient*, CallClient*> clients,
                           std::vector<EmulatedNetworkNode*> over_nodes,
                           DataSize overhead) {
  clients.second->route_overhead_.insert(
      {clients.second->endpoint()->GetId(), overhead});

  // Ensure that first is not connected
  network_emulation_manager_->ClearRoute(clients.first->endpoint(), over_nodes,
                                         clients.second->endpoint());

  network_emulation_manager_->CreateRoute(clients.first->endpoint(), over_nodes,
                                          clients.second->endpoint());
  clients.first->transport_.Connect(clients.second->transport_.local_address(),
                                    clients.second->endpoint()->GetId(),
                                    overhead);
}

SimulatedTimeClient* Scenario::CreateSimulatedTimeClient(
    std::string name,
    SimulatedTimeClientConfig config,
    std::vector<PacketStreamConfig> stream_configs,
    std::vector<EmulatedNetworkNode*> send_link,
    std::vector<EmulatedNetworkNode*> return_link) {
  // TODO(titovartem) refactor me
  uint64_t send_id = next_route_id_++;
  uint64_t return_id = next_route_id_++;
  SimulatedTimeClient* client = new SimulatedTimeClient(
      GetLogWriterFactory(name), config, stream_configs, send_link, return_link,
      send_id, return_id, Now());
  if (log_writer_factory_ && !name.empty() &&
      config.transport.state_log_interval.IsFinite()) {
    Every(config.transport.state_log_interval, [this, client]() {
      client->network_controller_factory_.LogCongestionControllerStats(Now());
    });
  }

  Every(client->GetNetworkControllerProcessInterval(),
        [this, client] { client->CongestionProcess(Now()); });
  Every(TimeDelta::ms(5), [this, client] { client->PacerProcess(Now()); });
  simulated_time_clients_.emplace_back(client);
  return client;
}

SimulationNode* Scenario::CreateSimulationNode(
    std::function<void(NetworkNodeConfig*)> config_modifier) {
  NetworkNodeConfig config;
  config_modifier(&config);
  return CreateSimulationNode(config);
}

SimulationNode* Scenario::CreateSimulationNode(NetworkNodeConfig config) {
  RTC_DCHECK(config.mode == NetworkNodeConfig::TrafficMode::kSimulation);

  SimulatedNetwork::Config sim_config =
      SimulationNode::CreateSimulationConfig(config);
  auto behavior = absl::make_unique<SimulatedNetwork>(sim_config);
  SimulatedNetwork* simulated_network = behavior.get();
  EmulatedNetworkNode* node = network_emulation_manager_->CreateEmulatedNode(
      std::move(behavior), config.packet_overhead.bytes_or(0));
  auto network_node =
      absl::WrapUnique(new SimulationNode(config, node, simulated_network));
  SimulationNode* sim_node = network_node.get();
  simulation_nodes_.emplace_back(std::move(network_node));

  return sim_node;
}

EmulatedNetworkNode* Scenario::CreateNetworkNode(
    NetworkNodeConfig config,
    std::unique_ptr<NetworkBehaviorInterface> behavior) {
  RTC_DCHECK(config.mode == NetworkNodeConfig::TrafficMode::kCustom);
  return network_emulation_manager_->CreateEmulatedNode(
      std::move(behavior), config.packet_overhead.bytes_or(0));
}

void Scenario::TriggerPacketBurst(std::vector<EmulatedNetworkNode*> over_nodes,
                                  size_t num_packets,
                                  size_t packet_size) {
  CrossTraffic* traffic = network_emulation_manager_->CreateCrossTraffic(
      over_nodes, absl::make_unique<IdleSendStrategy>());
  traffic->TriggerPacketBurst(num_packets, packet_size);
}

void Scenario::NetworkDelayedAction(
    std::vector<EmulatedNetworkNode*> over_nodes,
    size_t packet_size,
    std::function<void()> action) {
  action_receivers_.emplace_back(new ActionReceiver(action));
  // TODO(titovartem) rethink how to do it. It is bad to manipulate with
  // receivers here directly. Only network manager should do it.
  CrossTraffic* traffic = network_emulation_manager_->CreateCrossTraffic(
      over_nodes, absl::make_unique<IdleSendStrategy>());
  over_nodes.back()->RemoveReceiver(traffic->GetDestinationId());
  over_nodes.back()->SetReceiver(traffic->GetDestinationId(),
                                 action_receivers_.back().get());
  traffic->TriggerPacketBurst(1, packet_size);
}

CrossTraffic* Scenario::CreateCrossTraffic(
    std::vector<EmulatedNetworkNode*> over_nodes,
    std::function<void(CrossTrafficConfig*)> config_modifier) {
  CrossTrafficConfig cross_config;
  config_modifier(&cross_config);
  return CreateCrossTraffic(over_nodes, cross_config);
}

CrossTraffic* Scenario::CreateCrossTraffic(
    std::vector<EmulatedNetworkNode*> over_nodes,
    CrossTrafficConfig config) {
  std::unique_ptr<CrossTrafficSendStrategy> strategy = nullptr;
  if (config.mode == CrossTrafficConfig::Mode::kRandomWalk) {
    RandomWalkConfig cfg;
    cfg.random_seed = config.random_seed;
    cfg.peak_rate = config.peak_rate;
    cfg.min_packet_size = config.min_packet_size;
    cfg.min_packet_interval = config.min_packet_interval;
    cfg.update_interval = config.random_walk.update_interval;
    cfg.variance = config.random_walk.variance;
    cfg.bias = config.random_walk.bias;
    strategy = absl::make_unique<RandomWalkSendStrategy>(cfg);
  } else if (config.mode == CrossTrafficConfig::Mode::kPulsedPeaks) {
    PulsedPeaksConfig cfg;
    cfg.peak_rate = config.peak_rate;
    cfg.min_packet_size = config.min_packet_size;
    cfg.min_packet_interval = config.min_packet_interval;
    cfg.send_duration = config.pulsed.send_duration;
    cfg.hold_duration = config.pulsed.hold_duration;
    strategy = absl::make_unique<PulsedPeaksSendStrategy>(cfg);
  }
  RTC_CHECK(strategy);
  return network_emulation_manager_->CreateCrossTraffic(over_nodes,
                                                        std::move(strategy));
}

VideoStreamPair* Scenario::CreateVideoStream(
    std::pair<CallClient*, CallClient*> clients,
    std::function<void(VideoStreamConfig*)> config_modifier) {
  VideoStreamConfig config;
  config_modifier(&config);
  return CreateVideoStream(clients, config);
}

VideoStreamPair* Scenario::CreateVideoStream(
    std::pair<CallClient*, CallClient*> clients,
    VideoStreamConfig config) {
  std::unique_ptr<RtcEventLogOutput> quality_logger;
  if (config.analyzer.log_to_file)
    quality_logger = clients.first->GetLogWriter(".video_quality.txt");
  video_streams_.emplace_back(new VideoStreamPair(
      clients.first, clients.second, config, std::move(quality_logger)));
  return video_streams_.back().get();
}

AudioStreamPair* Scenario::CreateAudioStream(
    std::pair<CallClient*, CallClient*> clients,
    std::function<void(AudioStreamConfig*)> config_modifier) {
  AudioStreamConfig config;
  config_modifier(&config);
  return CreateAudioStream(clients, config);
}

AudioStreamPair* Scenario::CreateAudioStream(
    std::pair<CallClient*, CallClient*> clients,
    AudioStreamConfig config) {
  audio_streams_.emplace_back(
      new AudioStreamPair(clients.first, audio_encoder_factory_, clients.second,
                          audio_decoder_factory_, config));
  return audio_streams_.back().get();
}

RepeatedActivity* Scenario::Every(TimeDelta interval,
                                  std::function<void(TimeDelta)> function) {
  auto activity = absl::WrapUnique(new RepeatedActivity(interval, function));
  RepeatedActivity* out = activity.get();
  time_controller_->RegisterActivity(std::move(activity));
  return out;
}

RepeatedActivity* Scenario::Every(TimeDelta interval,
                                  std::function<void()> function) {
  auto function_with_argument = [function](TimeDelta) { function(); };
  return Every(interval, function_with_argument);
}

void Scenario::At(TimeDelta offset, std::function<void()> function) {
  pending_activities_.emplace_back(new PendingActivity{offset, function});
}

void Scenario::RunFor(TimeDelta duration) {
  RunUntil(Duration() + duration);
}

void Scenario::RunUntil(TimeDelta max_duration) {
  RunUntil(max_duration, TimeDelta::PlusInfinity(), []() { return false; });
}

void Scenario::RunUntil(TimeDelta max_duration,
                        TimeDelta poll_interval,
                        std::function<bool()> exit_function) {
  if (start_time_.IsInfinite())
    Start();

  std::atomic<bool> stopped{false};
  time_controller_->RegisterActivity(absl::make_unique<RepeatedActivity2>(
      [exit_function, &stopped, this](Timestamp at_time) {
        if (stopped) {
          // Don't stop controller, if it was stopped by different callback.
          return;
        }
        if (exit_function()) {
          time_controller_->Stop();
          stopped = true;
        }
      },
      poll_interval));
  time_controller_->RegisterActivity(absl::make_unique<DelayedActivity>(
      [this, &stopped](Timestamp at_time) {
        if (stopped) {
          // Don't stop controller, if it was stopped by different callback.
          return;
        }
        time_controller_->Stop();
        stopped = true;
      },
      max_duration - Duration()));

  time_controller_->Start();
  time_controller_->AwaitTermination();

  // std::printf("Start scenario loop\n");
  // rtc::Event done_;
  // while (!exit_function() && Duration() < max_duration) {
  //   Timestamp current_time = Now();
  //   TimeDelta duration = current_time - start_time_;
  //   Timestamp next_time = current_time + poll_interval;
  //   for (auto& activity : repeated_activities_) {
  //     activity->Poll(current_time);
  //     next_time = std::min(next_time, activity->NextTime());
  //   }
  //   for (auto activity = pending_activities_.begin();
  //        activity < pending_activities_.end(); activity++) {
  //     if (duration > (*activity)->after_duration) {
  //       (*activity)->function();
  //       pending_activities_.erase(activity);
  //     }
  //   }
  //   TimeDelta wait_time = next_time - current_time;
  //   if (real_time_mode_) {
  //     done_.Wait(wait_time.ms<int>());
  //   } else {
  //     sim_clock_.AdvanceTimeMicroseconds(wait_time.us());
  //     // The fake clock is quite slow to update, we only update it if logging
  //     is
  //     // turned on to save time.
  //     if (!log_writer_factory_)
  //       event_log_fake_clock_.SetTimeNanos(sim_clock_.TimeInMicroseconds() *
  //                                          1000);
  //   }
  // }
  // std::printf("End scenario loop\n");
  // network_emulation_manager_->Stop();
}

void Scenario::Start() {
  start_time_ = Timestamp::us(clock_->TimeInMicroseconds());
  for (auto& activity : repeated_activities_) {
    activity->SetStartTime(start_time_);
  }

  for (auto& stream_pair : video_streams_)
    stream_pair->receive()->Start();
  for (auto& stream_pair : audio_streams_)
    stream_pair->receive()->Start();
  for (auto& stream_pair : video_streams_) {
    if (stream_pair->config_.autostart) {
      stream_pair->send()->Start();
    }
  }
  for (auto& stream_pair : audio_streams_) {
    if (stream_pair->config_.autostart) {
      stream_pair->send()->Start();
    }
  }
}

void Scenario::Stop() {
  RTC_DCHECK(start_time_.IsFinite());
  for (auto& stream_pair : video_streams_)
    stream_pair->send()->sender_->thread()->Invoke<void>(
        RTC_FROM_HERE, [&]() { stream_pair->send()->send_stream_->Stop(); });
  for (auto& stream_pair : audio_streams_)
    stream_pair->send()->sender_->thread()->Invoke<void>(
        RTC_FROM_HERE, [&]() { stream_pair->send()->send_stream_->Stop(); });
  for (auto& stream_pair : video_streams_)
    stream_pair->receive()->receiver_->thread()->Invoke<void>(
        RTC_FROM_HERE,
        [&]() { stream_pair->receive()->receive_stream_->Stop(); });
  for (auto& stream_pair : audio_streams_)
    stream_pair->receive()->receiver_->thread()->Invoke<void>(
        RTC_FROM_HERE,
        [&]() { stream_pair->receive()->receive_stream_->Stop(); });
  start_time_ = Timestamp::PlusInfinity();
}

Timestamp Scenario::Now() {
  return Timestamp::us(clock_->TimeInMicroseconds());
}

TimeDelta Scenario::Duration() {
  if (start_time_.IsInfinite())
    return TimeDelta::Zero();
  return Now() - start_time_;
}

}  // namespace test
}  // namespace webrtc
