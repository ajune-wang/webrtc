/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_SCENARIO_H_
#define TEST_SCENARIO_SCENARIO_H_
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "test/scenario/audio_stream.h"
#include "test/scenario/call_client.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/network_node.h"
#include "test/scenario/scenario_config.h"
#include "test/scenario/video_stream.h"

namespace webrtc {
namespace test {
// RepeatedActivity is created by teh Scenario class and can be used to stop an
// running activity at runtime.
class RepeatedActivity {
 public:
  void Stop();

 private:
  friend class Scenario;
  RepeatedActivity(TimeDelta interval, std::function<void(TimeDelta)> function);

  void Poll(Timestamp time);
  void SetStartTime(Timestamp time);
  Timestamp NextTime();

  TimeDelta interval_;
  std::function<void(TimeDelta)> function_;
  Timestamp last_update_ = Timestamp::Infinity();
};

class PendingActivity {
 private:
  friend class Scenario;
  TimeDelta after_duration;
  std::function<void()> function;
};
struct PacketStreamConfig {
  int min_packet_rate = 30;
  DataSize max_packet_size = DataSize::bytes(1400);
  DataSize min_packet_size = DataSize::bytes(100);
  double packet_size_noise = 0;
  double initial_packet_size_multiplier = 2;
  DataRate max_data_rate = DataRate::Infinity();
};

class TransportControllerConfig {
  struct Rates {
    Rates();
    Rates(const Rates&);
    ~Rates();
    DataRate min_rate = DataRate::Zero();
    DataRate max_rate = DataRate::Infinity();
    DataRate start_rate = DataRate::kbps(300);
  } rates;
};

class ScenarioTransportController {
 public:
  ScenarioTransportController(TransportControllerConfig config, ) {}
};

class PacketStream {};

// Scenario is a class owning everything for a test scenario. It creates and
// holds network nodes, call clients and media streams. It also provides methods
// for changing behavior at runtime. Since it always keeps ownership of the
// created components, it generally returns non-owning pointers. It maintains
// the life of it's objects until it is destroyed.
// For methods accepting configuration structs, a modifier function interface is
// generally provided. This allows simple partial overriding of the default
// configuration.
class Scenario {
 public:
  Scenario();
  explicit Scenario(std::string log_path);
  explicit Scenario(std::string log_path, bool real_time);

  SimulationNode* CreateNetworkNode(NetworkNodeConfig config);
  SimulationNode* CreateNetworkNode(
      std::function<void(NetworkNodeConfig*)> config_modifier);
  NetworkNode* CreateNetworkNode(
      NetworkNodeConfig config,
      std::unique_ptr<NetworkSimulationInterface> simulation);

  CallClient* CreateClient(std::string name, CallClientConfig config);
  CallClient* CreateClient(
      std::string name,
      std::function<void(CallClientConfig*)> config_modifier);

  VideoStreamPair* CreateVideoStream(
      CallClient* sender,
      std::vector<NetworkNode*> send_link,
      CallClient* receiver,
      std::vector<NetworkNode*> return_link,
      std::function<void(VideoStreamConfig*)> config_modifier);
  VideoStreamPair* CreateVideoStream(CallClient* sender,
                                     std::vector<NetworkNode*> send_link,
                                     CallClient* receiver,
                                     std::vector<NetworkNode*> return_link,
                                     VideoStreamConfig config);

  AudioStreamPair* CreateAudioStream(
      CallClient* sender,
      std::vector<NetworkNode*> send_link,
      CallClient* receiver,
      std::vector<NetworkNode*> return_link,
      std::function<void(AudioStreamConfig*)> config_modifier);
  AudioStreamPair* CreateAudioStream(CallClient* sender,
                                     std::vector<NetworkNode*> send_link,
                                     CallClient* receiver,
                                     std::vector<NetworkNode*> return_link,
                                     AudioStreamConfig config);

  CrossTrafficSource* CreateCrossTraffic(
      std::vector<NetworkNode*> over_nodes,
      std::function<void(CrossTrafficConfig*)> config_modifier);
  CrossTrafficSource* CreateCrossTraffic(std::vector<NetworkNode*> over_nodes,
                                         CrossTrafficConfig config);

  // Runs the provided function with a fixed interval.
  RepeatedActivity* Every(TimeDelta interval,
                          std::function<void(TimeDelta)> function);
  RepeatedActivity* Every(TimeDelta interval, std::function<void()> function);

  // Runs the provided function after given duration has passed in a session.
  void At(TimeDelta offset, std::function<void()> function);

  // Runs the given function after one packet has been delivered over the given
  // nodes.
  void NetworkDelayedAction(std::vector<NetworkNode*> over_nodes,
                            size_t packet_size,
                            std::function<void()> action);

  // Runs the scenario for the given tim or until the exit function returns
  // true.
  void RunFor(TimeDelta duration);
  void RunUntil(TimeDelta max_duration,
                TimeDelta probe_interval,
                std::function<bool()> exit_function);

  // Triggers sending of packets over given nodes, bloating buffers.
  void TriggerBufferBloat(std::vector<NetworkNode*> over_nodes,
                          size_t num_packets,
                          size_t packet_size);

  ColumnPrinter TimePrinter();
  // Returns the current time.
  Timestamp Now();
  // Return the duration of the current session so far.
  TimeDelta Duration();

 private:
  NullReceiver null_receiver_;
  std::string base_filename_;
  const bool real_time_mode_;
  SimulatedClock sim_clock_;
  const Clock const* clock_;

  std::vector<std::unique_ptr<CallClient>> clients_;
  std::vector<std::unique_ptr<NetworkNode>> network_nodes_;
  std::vector<std::unique_ptr<CrossTrafficSource>> cross_traffic_sources_;
  std::vector<std::unique_ptr<VideoStreamPair>> video_streams_;
  std::vector<std::unique_ptr<AudioStreamPair>> audio_streams_;

  std::vector<std::unique_ptr<RepeatedActivity>> repeated_activities_;
  std::vector<std::unique_ptr<ActionReceiver>> action_receivers_;
  std::vector<std::unique_ptr<PendingActivity>> pending_activities_;

  int64_t next_receiver_id_ = 40000;
  rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory_;
  rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory_;

  Timestamp start_time_ = Timestamp::Infinity();
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_SCENARIO_H_
