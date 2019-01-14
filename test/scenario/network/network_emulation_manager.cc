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

constexpr int64_t kHeartbeatIntervalMs = 1;

}  // namespace

NetworkEmulationManager::NetworkEmulationManager(webrtc::Clock* clock)
    : clock_(clock),
      next_node_id_(1),
      state_(State::kIdle),
      task_queue_("network_emulation_manager") {}
NetworkEmulationManager::~NetworkEmulationManager() = default;

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

EndpointNode* NetworkEmulationManager::CreateEndpoint(
    rtc::IPAddress ip,
    EmulatedNetworkNode* send_node) {
  CheckIdle();

  auto node =
      absl::make_unique<EndpointNode>(next_node_id_++, ip, send_node, clock_);
  EndpointNode* out = node.get();
  endpoints_.push_back(std::move(node));
  return out;
}

void NetworkEmulationManager::CreateRoute(
    EndpointNode* from,
    std::vector<EmulatedNetworkNode*> via_nodes,
    EndpointNode* to) {
  CheckIdle();

  EmulatedNetworkNode* cur_node = from->GetSendNode();
  for (auto* node : via_nodes) {
    cur_node->SetReceiver(to->GetId(), node);
    cur_node = node;
  }
  cur_node->SetReceiver(to->GetId(), to);
  from->SetConnectedEndpointId(to->GetId());
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
    RTC_CHECK(state_ == State::kIdle);
    state_ = kRunning;
  }
  MakeHeartBeat();
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

void NetworkEmulationManager::MakeHeartBeat() {
  {
    rtc::CritScope crit(&lock_);
    if (state_ != kRunning) {
      state_ = kIdle;
      return;
    }
  }
  Timestamp current_time = Now();
  for (auto& node : network_nodes_) {
    node->Process(current_time);
  }
  task_queue_.PostDelayedTask([this]() { MakeHeartBeat(); },
                              kHeartbeatIntervalMs);
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
