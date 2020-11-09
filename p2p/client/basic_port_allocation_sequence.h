/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_CLIENT_BASIC_PORT_ALLOCATION_SEQUENCE_H_
#define P2P_CLIENT_BASIC_PORT_ALLOCATION_SEQUENCE_H_

#include <memory>
#include <string>
#include <vector>

#include "api/turn_customizer.h"
#include "p2p/base/port_allocator.h"
#include "p2p/client/basic_port_allocation_configuration.h"
#include "p2p/client/relay_port_factory_interface.h"
#include "p2p/client/turn_port_factory.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/checks.h"
#include "rtc_base/network.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread.h"

namespace cricket {
// Performs the allocation of ports, in a sequenced (timed) manner, for a given
// network and IP address.
class AllocationSequence : public rtc::MessageHandlerAutoCleanup,
                           public sigslot::has_slots<> {
 public:
  enum State {
    kInit,       // Initial state.
    kRunning,    // Started allocating ports.
    kStopped,    // Stopped from running.
    kCompleted,  // All ports are allocated.

    // kInit --> kRunning --> {kCompleted|kStopped}
  };
  AllocationSequence(BasicPortAllocatorSession* session,
                     rtc::Network* network,
                     PortConfiguration* config,
                     uint32_t flags);
  ~AllocationSequence() override;
  void Init();
  void Clear();
  void OnNetworkFailed();

  State state() const { return state_; }
  rtc::Network* network() const { return network_; }

  bool network_failed() const { return network_failed_; }
  void set_network_failed() { network_failed_ = true; }

  // Disables the phases for a new sequence that this one already covers for an
  // equivalent network setup.
  void DisableEquivalentPhases(rtc::Network* network,
                               PortConfiguration* config,
                               uint32_t* flags);

  // Starts and stops the sequence.  When started, it will continue allocating
  // new ports on its own timed schedule.
  void Start();
  void Stop();

  // MessageHandler
  void OnMessage(rtc::Message* msg) override;

  // Signal from AllocationSequence, when it's done with allocating ports.
  // This signal is useful, when port allocation fails which doesn't result
  // in any candidates. Using this signal BasicPortAllocatorSession can send
  // its candidate discovery conclusion signal. Without this signal,
  // BasicPortAllocatorSession doesn't have any event to trigger signal. This
  // can also be achieved by starting timer in BPAS.
  sigslot::signal1<AllocationSequence*> SignalPortAllocationComplete;

 protected:
  // For testing.
  void CreateTurnPort(const RelayServerConfig& config);

 private:
  typedef std::vector<ProtocolType> ProtocolList;

  bool IsFlagSet(uint32_t flag) { return ((flags_ & flag) != 0); }
  void CreateUDPPorts();
  void CreateTCPPorts();
  void CreateStunPorts();
  void CreateRelayPorts();

  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const char* data,
                    size_t size,
                    const rtc::SocketAddress& remote_addr,
                    const int64_t& packet_time_us);

  void OnPortDestroyed(PortInterface* port);

  BasicPortAllocatorSession* session_;
  bool network_failed_ = false;
  rtc::Network* network_;
  // Compared with the new best IP in DisableEquivalentPhases.
  rtc::IPAddress previous_best_ip_;
  PortConfiguration* config_;
  State state_;
  uint32_t flags_;
  ProtocolList protocols_;
  std::unique_ptr<rtc::AsyncPacketSocket> udp_socket_;
  // There will be only one udp port per AllocationSequence.
  UDPPort* udp_port_;
  std::vector<Port*> relay_ports_;
  int phase_;
};

}  // namespace cricket

#endif  // P2P_CLIENT_BASIC_PORT_ALLOCATION_SEQUENCE_H_
