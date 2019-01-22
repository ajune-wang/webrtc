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

  // Creates and binds local socket to specified IP address
  void Bind(EndpointNode* endpoint);

  // Connected created local socket to specified remote address.
  void Connect(rtc::SocketAddress remote_addr,
               uint64_t dest_endpoint_id,
               DataSize packet_overhead);

  DataSize packet_overhead() {
    rtc::CritScope crit(&crit_sect_);
    return packet_overhead_;
  }

 private:
  rtc::CriticalSection crit_sect_;

  rtc::SocketFactory* const socket_factory_;
  std::function<void(rtc::CopyOnWriteBuffer, int64_t timestamp)> receiver_;
  // Socket to send outgoing packets and receive incoming
  std::vector<rtc::AsyncSocket*> sockets_;
  absl::optional<uint64_t> current_endpoint_id_;
  const Clock* const sender_clock_;
  Call* const sender_call_;

  DataSize packet_overhead_ RTC_GUARDED_BY(crit_sect_) = DataSize::Zero();
};

}  // namespace test
}  // namespace webrtc
#endif  // TEST_SCENARIO_NETWORK_NODE_H_
