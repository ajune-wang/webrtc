/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_NETWORK_NODE_H_
#define TEST_SCENARIO_NETWORK_NODE_H_

#include <deque>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/call/transport.h"
#include "api/units/timestamp.h"
#include "call/call.h"
#include "call/simulated_network.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/network/network_emulation.h"
#include "test/scenario/scenario_config.h"

namespace webrtc {
namespace test {

class NullReceiver : public EmulatedNetworkReceiverInterface {
 public:
  void OnPacketReceived(EmulatedIpPacket packet) override;
};
class ActionReceiver : public EmulatedNetworkReceiverInterface {
 public:
  explicit ActionReceiver(std::function<void()> action);
  virtual ~ActionReceiver() = default;

  void OnPacketReceived(EmulatedIpPacket packet) override;

 private:
  std::function<void()> action_;
};

// SimulationNode is a EmulatedNetworkNode that expose an interface for changing
// run time behavior of the underlying simulation.
class SimulationNode {
 public:
  void UpdateConfig(std::function<void(NetworkNodeConfig*)> modifier);
  void PauseTransmissionUntil(Timestamp until);
  ColumnPrinter ConfigPrinter() const;
  EmulatedNetworkNode* node() const;

 private:
  friend class Scenario;

  SimulationNode(NetworkNodeConfig config,
                 EmulatedNetworkNode* node,
                 SimulatedNetwork* simulation);
  static SimulatedNetwork::Config CreateSimulationConfig(
      NetworkNodeConfig config);

  SimulatedNetwork* const simulated_network_;
  NetworkNodeConfig config_;
  EmulatedNetworkNode* const node_;
};

class NetworkNodeTransport : public Transport, public sigslot::has_slots<> {
 public:
  NetworkNodeTransport(
      const Clock* sender_clock,
      Call* sender_call,
      rtc::SocketFactory* socket_factory,
      std::function<void(rtc::CopyOnWriteBuffer, int64_t timestamp)> receiver);
  ~NetworkNodeTransport() override;

  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override;
  bool SendRtcp(const uint8_t* packet, size_t length) override;

  rtc::SocketAddress local_address() const;
  void OnPacketReceived(rtc::AsyncSocket* socket);

  // Bind and Connect supposed to be called sequentially on the same thread.
  // It is not supposed, that any of these two methods can be called
  // concurrently and also during active data send/receive.
  // Creates and binds local socket to specified IP address
  void Bind(EndpointNode* local_endpoint, EndpointNode* remote_endpoint);
  // Connected created local socket to specified remote address.
  void Connect(rtc::SocketAddress remote_addr, DataSize packet_overhead);

  DataSize packet_overhead() {
    rtc::CritScope crit(&lock_);
    return packet_overhead_;
  }

 private:
  rtc::CriticalSection lock_;

  rtc::SocketFactory* const socket_factory_;
  std::function<void(rtc::CopyOnWriteBuffer, int64_t timestamp)> receiver_;
  const Clock* const sender_clock_;
  Call* const sender_call_;

  // Sockets to send outgoing packets and receive incoming. Current operational
  // one is the last one. The rest are kept to receive the data, that can still
  // be in the network.
  std::vector<rtc::AsyncSocket*> sockets_ RTC_GUARDED_BY(lock_);
  // Transport keep connection between sockets, that are binded to endpoints.
  // If current socket is used for connection from endpoint A to B, we
  // should reuse it for connection from B to A. Otherwise we should create
  // new socket.
  absl::optional<EndpointNode*> current_local_endpoint_ = absl::nullopt;
  absl::optional<EndpointNode*> current_remote_endpoint_ = absl::nullopt;
  DataSize packet_overhead_ RTC_GUARDED_BY(lock_) = DataSize::Zero();
};

}  // namespace test
}  // namespace webrtc
#endif  // TEST_SCENARIO_NETWORK_NODE_H_
