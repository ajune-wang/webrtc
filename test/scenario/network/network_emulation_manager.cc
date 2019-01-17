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

constexpr int64_t kPacketProcessingIntervalMs = 1;
class NullReceiver : public EmulatedNetworkReceiverInterface {
 public:
  void OnPacketReceived(EmulatedIpPacket packet) override{};
};

}  // namespace

NetworkEmulationManager::NetworkEmulationManager(webrtc::Clock* clock)
    : clock_(clock),
      next_node_id_(1),
      dev_null_receiver_(absl::make_unique<NullReceiver>()),
      state_(State::kIdle),
      task_queue_("network_emulation_manager") {}
NetworkEmulationManager::~NetworkEmulationManager() {
  Stop();
}

EmulatedNetworkNode* NetworkEmulationManager::CreateEmulatedNode(
    std::unique_ptr<NetworkBehaviorInterface> network_behavior,
    size_t packet_overhead) {
  CheckIdle();

  auto node = absl::make_unique<EmulatedNetworkNode>(
      std::move(network_behavior), packet_overhead);
  EmulatedNetworkNode* out = node.get();
  network_nodes_.push_back(std::move(node));
  return out;
}

EndpointNode* NetworkEmulationManager::CreateEndpoint(rtc::IPAddress ip) {
  CheckIdle();

  auto node = absl::make_unique<EndpointNode>(next_node_id_++, ip, clock_);
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
  // From shouldn't be attached to some network.
  RTC_CHECK(!from->GetSendNode());
  CheckIdle();

  from->SetSendNode(via_nodes[0]);
  EmulatedNetworkNode* cur_node = via_nodes[0];
  for (size_t i = 1; i < via_nodes.size(); ++i) {
    cur_node->SetReceiver(to->GetId(), via_nodes[i]);
    cur_node = via_nodes[i];
  }
  cur_node->SetReceiver(to->GetId(), to);
  from->SetConnectedEndpointId(to->GetId());
}

CrossTraffic* NetworkEmulationManager::CreateCrossTraffic(
    std::vector<EmulatedNetworkNode*> via_nodes,
    std::unique_ptr<CrossTrafficSendStrategy> strategy) {
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
      clock_, via_nodes[0], endpoint_id, std::move(strategy));
  CrossTraffic* out = cross_traffic.get();
  cross_traffics_.push_back(std::move(cross_traffic));
  return out;
}

rtc::Thread* NetworkEmulationManager::CreateNetworkThread(
    std::vector<EndpointNode*> endpoints) {
  CheckIdle();

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
  {
    rtc::CritScope crit(&lock_);
    if (state_ == State::kStopping) {
      // Manager was stopped, but ProcessNetworkPackets was not invoked yet
      state_ = kRunning;
      std::printf("Network manager continued\n");
      return;
    }
    RTC_CHECK(state_ == State::kIdle);
    state_ = kRunning;
  }
  std::printf("Network manager started\n");
  ProcessNetworkPackets();
}

void NetworkEmulationManager::Stop() {
  rtc::CritScope crit(&lock_);
  state_ = State::kStopping;
}

FakeNetworkSocketServer* NetworkEmulationManager::CreateSocketServer(
    std::vector<EndpointNode*> endpoints) {
  CheckIdle();

  auto socket_server =
      absl::make_unique<FakeNetworkSocketServer>(clock_, endpoints);
  FakeNetworkSocketServer* out = socket_server.get();
  socket_servers_.push_back(std::move(socket_server));
  return out;
}

void NetworkEmulationManager::ProcessNetworkPackets() {
  {
    rtc::CritScope crit(&lock_);
    if (state_ != kRunning) {
      state_ = kIdle;
      std::printf("Network manager stopped\n");
      return;
    }
  }
  if (last_process_time_.IsMinusInfinity()) {
    last_process_time_ = Now();
  }
  Timestamp current_time = Now();
  for (auto& cross_traffic : cross_traffics_) {
    cross_traffic->Process(current_time, current_time - last_process_time_);
  }
  for (auto& node : network_nodes_) {
    node->Process(current_time);
  }
  last_process_time_ = Now();
  task_queue_.PostDelayedTask([this]() { ProcessNetworkPackets(); },
                              kPacketProcessingIntervalMs);
}

void NetworkEmulationManager::CheckIdle() const {
  rtc::CritScope crit(&lock_);
  RTC_CHECK(state_ == State::kIdle);
}

Timestamp NetworkEmulationManager::Now() const {
  return Timestamp::us(clock_->TimeInMicroseconds());
}

}  // namespace test
}  // namespace webrtc
