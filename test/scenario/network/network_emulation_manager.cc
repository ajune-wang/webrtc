/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/network/network_emulation_manager.h"

#include <algorithm>
#include <memory>

#include "absl/memory/memory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {
namespace test {
namespace {

class NullReceiver : public EmulatedNetworkReceiverInterface {
 public:
  void OnPacketReceived(EmulatedIpPacket packet) override{};
};

}  // namespace

NetworkEmulationManager::NetworkEmulationManager(
    TimeController* time_controller)
    : time_controller_(time_controller),
      next_node_id_(1),
      dev_null_receiver_(absl::make_unique<NullReceiver>()) {}
NetworkEmulationManager::~NetworkEmulationManager() {
  Stop();
}

EmulatedNetworkNode* NetworkEmulationManager::CreateEmulatedNode(
    std::unique_ptr<NetworkBehaviorInterface> network_behavior,
    size_t packet_overhead) {
  auto node = absl::make_unique<EmulatedNetworkNode>(
      std::move(network_behavior), packet_overhead);
  EmulatedNetworkNode* out = node.get();
  network_nodes_.push_back(std::move(node));
  time_controller_->RegisterActivity(absl::make_unique<RepeatedActivity2>(
      [out](Timestamp at_time) { out->Process(at_time); }, TimeDelta::ms(1)));
  return out;
}

EndpointNode* NetworkEmulationManager::CreateEndpoint(rtc::IPAddress ip) {
  auto node = absl::make_unique<EndpointNode>(next_node_id_++, ip,
                                              time_controller_->clock());
  EndpointNode* out = node.get();
  endpoints_.push_back(std::move(node));
  return out;
}

void NetworkEmulationManager::CreateRoute(
    EndpointNode* from,
    std::vector<EmulatedNetworkNode*> via_nodes,
    EndpointNode* to) {
  // Because endpoint has no send node by default at least one should be
  // provided here.
  RTC_CHECK(!via_nodes.empty());

  from->SetSendNode(via_nodes[0]);
  EmulatedNetworkNode* cur_node = via_nodes[0];
  for (size_t i = 1; i < via_nodes.size(); ++i) {
    cur_node->SetReceiver(to->GetId(), via_nodes[i]);
    cur_node = via_nodes[i];
  }
  cur_node->SetReceiver(to->GetId(), to);
  from->SetConnectedEndpointId(to->GetId());
}

void NetworkEmulationManager::ClearRoute(
    EndpointNode* from,
    std::vector<EmulatedNetworkNode*> via_nodes,
    EndpointNode* to) {
  // Remove receiver from intermediate nodes.
  for (auto* node : via_nodes) {
    node->RemoveReceiver(to->GetId());
  }
  // Detach endpoint from current send node.
  if (from->GetSendNode()) {
    from->GetSendNode()->RemoveReceiver(to->GetId());
    from->SetSendNode(nullptr);
  }
}

CrossTraffic* NetworkEmulationManager::CreateCrossTraffic(
    std::vector<EmulatedNetworkNode*> via_nodes) {
  RTC_CHECK(!via_nodes.empty());
  uint64_t endpoint_id = next_node_id_++;

  // Setup a route via specified nodes.
  EmulatedNetworkNode* cur_node = via_nodes[0];
  for (size_t i = 1; i < via_nodes.size(); ++i) {
    cur_node->SetReceiver(endpoint_id, via_nodes[i]);
    cur_node = via_nodes[i];
  }
  cur_node->SetReceiver(endpoint_id, dev_null_receiver_.get());

  std::unique_ptr<CrossTraffic> cross_traffic = absl::make_unique<CrossTraffic>(
      time_controller_->clock(), via_nodes[0], endpoint_id);
  CrossTraffic* out = cross_traffic.get();
  cross_traffics_.push_back(std::move(cross_traffic));
  return out;
}

RandomWalkCrossTraffic* NetworkEmulationManager::CreateRandomWalkCrossTraffic(
    CrossTraffic* cross_traffic,
    RandomWalkConfig config) {
  auto traffic = absl::make_unique<RandomWalkCrossTraffic>(std::move(config),
                                                           cross_traffic);
  RandomWalkCrossTraffic* out = traffic.get();
  random_cross_traffics_.push_back(std::move(traffic));
  time_controller_->RegisterActivity(absl::make_unique<RepeatedActivity2>(
      [out](Timestamp at_time) { out->Process(at_time); }, TimeDelta::ms(1)));
  return out;
}

PulsedPeaksCrossTraffic* NetworkEmulationManager::CreatePulsedPeaksCrossTraffic(
    CrossTraffic* cross_traffic,
    PulsedPeaksConfig config) {
  auto traffic = absl::make_unique<PulsedPeaksCrossTraffic>(std::move(config),
                                                            cross_traffic);
  PulsedPeaksCrossTraffic* out = traffic.get();
  pulsed_cross_traffics_.push_back(std::move(traffic));
  time_controller_->RegisterActivity(absl::make_unique<RepeatedActivity2>(
      [out](Timestamp at_time) { out->Process(at_time); }, TimeDelta::ms(1)));
  return out;
}

rtc::Thread* NetworkEmulationManager::CreateNetworkThread(
    std::vector<EndpointNode*> endpoints) {
  FakeNetworkSocketServer* socket_server = CreateSocketServer(endpoints);
  std::unique_ptr<rtc::Thread> network_thread =
      absl::make_unique<rtc::Thread>(socket_server);
  network_thread->SetName("network_thread" + std::to_string(threads_.size()),
                          nullptr);
  network_thread->Start();
  rtc::Thread* out = network_thread.get();
  threads_.push_back(std::move(network_thread));
  return out;
}

void NetworkEmulationManager::Start() {
  time_controller_->Start();
}

void NetworkEmulationManager::Stop() {
  time_controller_->Stop();
}

FakeNetworkSocketServer* NetworkEmulationManager::CreateSocketServer(
    std::vector<EndpointNode*> endpoints) {
  auto socket_server = absl::make_unique<FakeNetworkSocketServer>(
      time_controller_->clock(), endpoints);
  FakeNetworkSocketServer* out = socket_server.get();
  socket_servers_.push_back(std::move(socket_server));
  return out;
}

}  // namespace test
}  // namespace webrtc
