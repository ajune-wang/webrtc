/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <utility>

#include "api/test/network_emulation_manager.h"
#include "call/simulated_network.h"

namespace webrtc {

NetworkEmulationManager::NetworkNodeBuilder::NetworkNodeBuilder(
    NetworkEmulationManager* net)
    : net_(net) {}

NetworkEmulationManager::NetworkNodeBuilder&
NetworkEmulationManager::NetworkNodeBuilder::config(
    SimulatedNetworkInterface::Config config) {
  config_ = config;
  return *this;
}

NetworkEmulationManager::NetworkNodeBuilder&
NetworkEmulationManager::NetworkNodeBuilder::delay_ms(int queue_delay_ms) {
  config_.queue_delay_ms = queue_delay_ms;
  return *this;
}

NetworkEmulationManager::NetworkNodeBuilder&
NetworkEmulationManager::NetworkNodeBuilder::capacity_kbps(
    int link_capacity_kbps) {
  config_.link_capacity_kbps = link_capacity_kbps;
  return *this;
}

NetworkEmulationManager::NetworkNodeBuilder&
NetworkEmulationManager::NetworkNodeBuilder::capacity_Mbps(
    int link_capacity_Mbps) {
  config_.link_capacity_kbps = link_capacity_Mbps * 1000;
  return *this;
}

NetworkEmulationManager::NetworkNodeBuilder&
NetworkEmulationManager::NetworkNodeBuilder::loss(double loss_rate) {
  config_.loss_percent = std::round(loss_rate * 100);
  return *this;
}

NetworkEmulationManager::SimulatedNetworkNode
NetworkEmulationManager::NetworkNodeBuilder::Build() const {
  RTC_DCHECK(net_);
  return Build(net_);
}

NetworkEmulationManager::SimulatedNetworkNode
NetworkEmulationManager::NetworkNodeBuilder::Build(
    NetworkEmulationManager* net) const {
  SimulatedNetworkNode res;
  auto behavior = std::make_unique<SimulatedNetwork>(config_);
  res.simulation = behavior.get();
  res.node = net->CreateEmulatedNode(std::move(behavior));
  return res;
}
}  // namespace webrtc
