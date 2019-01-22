/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/network_node.h"

#include <algorithm>
#include <vector>

#include "absl/memory/memory.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace test {

void NullReceiver::OnPacketReceived(EmulatedIpPacket packet) {}

ActionReceiver::ActionReceiver(std::function<void()> action)
    : action_(action) {}

void ActionReceiver::OnPacketReceived(EmulatedIpPacket packet) {
  action_();
}

SimulatedNetwork::Config SimulationNode::CreateSimulationConfig(
    NetworkNodeConfig config) {
  SimulatedNetwork::Config sim_config;
  sim_config.link_capacity_kbps = config.simulation.bandwidth.kbps_or(0);
  sim_config.loss_percent = config.simulation.loss_rate * 100;
  sim_config.queue_delay_ms = config.simulation.delay.ms();
  sim_config.delay_standard_deviation_ms = config.simulation.delay_std_dev.ms();
  return sim_config;
}

void SimulationNode::UpdateConfig(
    std::function<void(NetworkNodeConfig*)> modifier) {
  modifier(&config_);
  SimulatedNetwork::Config sim_config = CreateSimulationConfig(config_);
  simulated_network_->SetConfig(sim_config);
}

void SimulationNode::PauseTransmissionUntil(Timestamp until) {
  simulated_network_->PauseTransmissionUntil(until.us());
}

ColumnPrinter SimulationNode::ConfigPrinter() const {
  return ColumnPrinter::Lambda("propagation_delay capacity loss_rate",
                               [this](rtc::SimpleStringBuilder& sb) {
                                 sb.AppendFormat(
                                     "%.3lf %.0lf %.2lf",
                                     config_.simulation.delay.seconds<double>(),
                                     config_.simulation.bandwidth.bps() / 8.0,
                                     config_.simulation.loss_rate);
                               });
}

EmulatedNetworkNode* SimulationNode::node() const {
  return node_;
}

SimulationNode::SimulationNode(NetworkNodeConfig config,
                               EmulatedNetworkNode* node,
                               SimulatedNetwork* simulation)
    : simulated_network_(simulation), config_(config), node_(node) {}

NetworkNodeTransport::NetworkNodeTransport(
    const Clock* sender_clock,
    Call* sender_call,
    rtc::SocketFactory* socket_factory,
    std::function<void(rtc::CopyOnWriteBuffer, int64_t timestamp)> receiver)
    : socket_factory_(socket_factory),
      receiver_(receiver),
      sender_clock_(sender_clock),
      sender_call_(sender_call) {}

NetworkNodeTransport::~NetworkNodeTransport() {
  for (auto* socket : sockets_) {
    delete socket;
  }
}

rtc::SocketAddress NetworkNodeTransport::local_address() const {
  return sockets_.back()->GetLocalAddress();
}

bool NetworkNodeTransport::SendRtp(const uint8_t* packet,
                                   size_t length,
                                   const PacketOptions& options) {
  int64_t send_time_ms = sender_clock_->TimeInMilliseconds();
  rtc::SentPacket sent_packet;
  sent_packet.packet_id = options.packet_id;
  sent_packet.info.included_in_feedback = options.included_in_feedback;
  sent_packet.info.included_in_allocation = options.included_in_allocation;
  sent_packet.send_time_ms = send_time_ms;
  sent_packet.info.packet_size_bytes = length;
  sent_packet.info.packet_type = rtc::PacketType::kData;
  sender_call_->OnSentPacket(sent_packet);

  rtc::CritScope crit(&crit_sect_);
  if (sockets_.empty())
    return false;
  rtc::CopyOnWriteBuffer buffer(packet, length,
                                length + packet_overhead_.bytes());
  buffer.SetSize(length + packet_overhead_.bytes());

  sockets_.back()->Send(buffer.data(), buffer.size());
  return true;
}

bool NetworkNodeTransport::SendRtcp(const uint8_t* packet, size_t length) {
  rtc::CopyOnWriteBuffer buffer(packet, length);
  rtc::CritScope crit(&crit_sect_);
  buffer.SetSize(length + packet_overhead_.bytes());
  if (sockets_.empty())
    return false;
  sockets_.back()->Send(buffer.data(), buffer.size());
  return true;
}

void NetworkNodeTransport::OnPacketReceived(rtc::AsyncSocket* socket) {
  size_t capacity = 64 * 1024;
  uint8_t buffer[capacity];
  int64_t timestamp;
  size_t len = socket->Recv(buffer, capacity, &timestamp);
  receiver_(rtc::CopyOnWriteBuffer(buffer, len, len), timestamp);
}

void NetworkNodeTransport::Bind(EndpointNode* endpoint) {
  rtc::CritScope crit(&crit_sect_);
  if (current_endpoint_id_ &&
      current_endpoint_id_.value() == endpoint->GetId()) {
    // Current socket is already attached to required endpoint.
    return;
  }
  current_endpoint_id_ = endpoint->GetId();
  rtc::AsyncSocket* socket =
      socket_factory_->CreateAsyncSocket(AF_INET, SOCK_DGRAM);
  // Bind it to local endpoint IP. Port is 0, so endpoint will pick one.
  RTC_CHECK_EQ(
      socket->Bind(rtc::SocketAddress(endpoint->GetPeerLocalAddress(), 0)), 0);
  sockets_.push_back(socket);
}

void NetworkNodeTransport::Connect(rtc::SocketAddress remote_addr,
                                   uint64_t dest_endpoint_id,
                                   DataSize packet_overhead) {
  rtc::CritScope crit(&crit_sect_);

  // TODO(titovartem) DCHECK state of the last socket.
  RTC_CHECK_EQ(sockets_.back()->Connect(remote_addr), 0);
  sockets_.back()->SignalReadEvent.connect(
      this, &NetworkNodeTransport::OnPacketReceived);

  packet_overhead_ = packet_overhead;

  rtc::NetworkRoute route;
  route.connected = true;
  route.local_network_id = dest_endpoint_id;
  route.remote_network_id = dest_endpoint_id;
  std::string transport_name = "dummy";
  sender_call_->GetTransportControllerSend()->OnNetworkRouteChanged(
      transport_name, route);
}

}  // namespace test
}  // namespace webrtc
