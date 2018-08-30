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
class RepeatedActivity {
 public:
  RepeatedActivity(TimeDelta interval, std::function<void(TimeDelta)> function);
  void Stop();

 protected:
  friend class Scenario;
  void Poll(Timestamp time);
  void SetStartTime(Timestamp time);
  Timestamp NextTime();

 private:
  TimeDelta interval_;
  std::function<void(TimeDelta)> function_;
  Timestamp last_update_ = Timestamp::Infinity();
};

// Scenario manages lifetimes etc.
class Scenario {
 public:
  Scenario();
  explicit Scenario(std::string log_path);

  ColumnPrinter TimePrinter();

  CallClient* CreateClient(std::string name, CallClientConfig config);
  CallClient* CreateClient(
      std::string name,
      std::function<void(CallClientConfig*)> config_modifier);
  SimulationNode* CreateNetworkNode(NetworkNodeConfig config);
  SimulationNode* CreateNetworkNode(
      std::function<void(NetworkNodeConfig*)> config_modifier);
  NetworkNode* CreateNetworkNode(
      NetworkNodeConfig config,
      std::unique_ptr<NetworkSimulationInterface> simulation);

  void BufferBloat(std::vector<NetworkNode*> over_nodes,
                   size_t num_packets,
                   size_t packet_size);
  void NetworkDelayedAction(std::vector<NetworkNode*> over_nodes,
                            size_t packet_size,
                            std::function<void()> action);

  CrossTrafficSource* CreateCrossTraffic(
      std::vector<NetworkNode*> over_nodes,
      std::function<void(CrossTrafficConfig*)> config_modifier);
  CrossTrafficSource* CreateCrossTraffic(std::vector<NetworkNode*> over_nodes,
                                         CrossTrafficConfig config);

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

  RepeatedActivity* Every(TimeDelta interval,
                          std::function<void(TimeDelta)> function);
  RepeatedActivity* Every(TimeDelta interval, std::function<void()> function);

  void At(TimeDelta offset, std::function<void()> function);

  void RunFor(TimeDelta duration);
  void RunUntil(TimeDelta max_duration,
                TimeDelta probe_interval,
                std::function<bool()> exit_function);
  Timestamp Now();
  TimeDelta Duration();

 private:
  struct PendingActivity {
    TimeDelta after_duration;
    std::function<void()> function;
  };

  NullReceiver null_receiver_;
  std::string base_filename_;

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
