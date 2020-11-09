
/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>
#include <vector>

#include "api/turn_customizer.h"
#include "p2p/base/port_allocator.h"
#include "p2p/client/relay_port_factory_interface.h"
#include "p2p/client/turn_port_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/network.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread.h"

namespace cricket {

AllocationSequence::AllocationSequence(BasicPortAllocatorSession* session,
                                       rtc::Network* network,
                                       PortConfiguration* config,
                                       uint32_t flags)
    : session_(session),
      network_(network),
      config_(config),
      state_(kInit),
      flags_(flags),
      udp_socket_(),
      udp_port_(NULL),
      phase_(0) {}

void AllocationSequence::Init() {
  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
    udp_socket_.reset(session_->socket_factory()->CreateUdpSocket(
        rtc::SocketAddress(network_->GetBestIP(), 0),
        session_->allocator()->min_port(), session_->allocator()->max_port()));
    if (udp_socket_) {
      udp_socket_->SignalReadPacket.connect(this,
                                            &AllocationSequence::OnReadPacket);
    }
    // Continuing if |udp_socket_| is NULL, as local TCP and RelayPort using TCP
    // are next available options to setup a communication channel.
  }
}

void AllocationSequence::Clear() {
  udp_port_ = NULL;
  relay_ports_.clear();
}

void AllocationSequence::OnNetworkFailed() {
  RTC_DCHECK(!network_failed_);
  network_failed_ = true;
  // Stop the allocation sequence if its network failed.
  Stop();
}

AllocationSequence::~AllocationSequence() {
  session_->network_thread()->Clear(this);
}

void AllocationSequence::DisableEquivalentPhases(rtc::Network* network,
                                                 PortConfiguration* config,
                                                 uint32_t* flags) {
  if (network_failed_) {
    // If the network of this allocation sequence has ever become failed,
    // it won't be equivalent to the new network.
    return;
  }

  if (!((network == network_) && (previous_best_ip_ == network->GetBestIP()))) {
    // Different network setup; nothing is equivalent.
    return;
  }

  // Else turn off the stuff that we've already got covered.

  // Every config implicitly specifies local, so turn that off right away if we
  // already have a port of the corresponding type. Look for a port that
  // matches this AllocationSequence's network, is the right protocol, and
  // hasn't encountered an error.
  // TODO(deadbeef): This doesn't take into account that there may be another
  // AllocationSequence that's ABOUT to allocate a UDP port, but hasn't yet.
  // This can happen if, say, there's a network change event right before an
  // application-triggered ICE restart. Hopefully this problem will just go
  // away if we get rid of the gathering "phases" though, which is planned.
  //
  //
  // PORTALLOCATOR_DISABLE_UDP is used to disable a Port from gathering the host
  // candidate (and srflx candidate if Port::SharedSocket()), and we do not want
  // to disable the gathering of these candidates just becaue of an existing
  // Port over PROTO_UDP, namely a TurnPort over UDP.
  if (absl::c_any_of(session_->ports_,
                     [this](const BasicPortAllocatorSession::PortData& p) {
                       return !p.pruned() && p.port()->Network() == network_ &&
                              p.port()->GetProtocol() == PROTO_UDP &&
                              p.port()->Type() == LOCAL_PORT_TYPE && !p.error();
                     })) {
    *flags |= PORTALLOCATOR_DISABLE_UDP;
  }
  // Similarly we need to check both the protocol used by an existing Port and
  // its type.
  if (absl::c_any_of(session_->ports_,
                     [this](const BasicPortAllocatorSession::PortData& p) {
                       return !p.pruned() && p.port()->Network() == network_ &&
                              p.port()->GetProtocol() == PROTO_TCP &&
                              p.port()->Type() == LOCAL_PORT_TYPE && !p.error();
                     })) {
    *flags |= PORTALLOCATOR_DISABLE_TCP;
  }

  if (config_ && config) {
    // We need to regather srflx candidates if either of the following
    // conditions occurs:
    //  1. The STUN servers are different from the previous gathering.
    //  2. We will regather host candidates, hence possibly inducing new NAT
    //     bindings.
    if (config_->StunServers() == config->StunServers() &&
        (*flags & PORTALLOCATOR_DISABLE_UDP)) {
      // Already got this STUN servers covered.
      *flags |= PORTALLOCATOR_DISABLE_STUN;
    }
    if (!config_->relays.empty()) {
      // Already got relays covered.
      // NOTE: This will even skip a _different_ set of relay servers if we
      // were to be given one, but that never happens in our codebase. Should
      // probably get rid of the list in PortConfiguration and just keep a
      // single relay server in each one.
      *flags |= PORTALLOCATOR_DISABLE_RELAY;
    }
  }
}

void AllocationSequence::Start() {
  state_ = kRunning;
  session_->network_thread()->Post(RTC_FROM_HERE, this, MSG_ALLOCATION_PHASE);
  // Take a snapshot of the best IP, so that when DisableEquivalentPhases is
  // called next time, we enable all phases if the best IP has since changed.
  previous_best_ip_ = network_->GetBestIP();
}

void AllocationSequence::Stop() {
  // If the port is completed, don't set it to stopped.
  if (state_ == kRunning) {
    state_ = kStopped;
    session_->network_thread()->Clear(this, MSG_ALLOCATION_PHASE);
  }
}

void AllocationSequence::OnMessage(rtc::Message* msg) {
  RTC_DCHECK(rtc::Thread::Current() == session_->network_thread());
  RTC_DCHECK(msg->message_id == MSG_ALLOCATION_PHASE);

  const char* const PHASE_NAMES[kNumPhases] = {"Udp", "Relay", "Tcp"};

  // Perform all of the phases in the current step.
  RTC_LOG(LS_INFO) << network_->ToString()
                   << ": Allocation Phase=" << PHASE_NAMES[phase_];

  switch (phase_) {
    case PHASE_UDP:
      CreateUDPPorts();
      CreateStunPorts();
      break;

    case PHASE_RELAY:
      CreateRelayPorts();
      break;

    case PHASE_TCP:
      CreateTCPPorts();
      state_ = kCompleted;
      break;

    default:
      RTC_NOTREACHED();
  }

  if (state() == kRunning) {
    ++phase_;
    session_->network_thread()->PostDelayed(RTC_FROM_HERE,
                                            session_->allocator()->step_delay(),
                                            this, MSG_ALLOCATION_PHASE);
  } else {
    // If all phases in AllocationSequence are completed, no allocation
    // steps needed further. Canceling  pending signal.
    session_->network_thread()->Clear(this, MSG_ALLOCATION_PHASE);
    SignalPortAllocationComplete(this);
  }
}

void AllocationSequence::CreateUDPPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_UDP)) {
    RTC_LOG(LS_VERBOSE) << "AllocationSequence: UDP ports disabled, skipping.";
    return;
  }

  // TODO(mallinath) - Remove UDPPort creating socket after shared socket
  // is enabled completely.
  std::unique_ptr<UDPPort> port;
  bool emit_local_candidate_for_anyaddress =
      !IsFlagSet(PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE);
  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET) && udp_socket_) {
    port = UDPPort::Create(
        session_->network_thread(), session_->socket_factory(), network_,
        udp_socket_.get(), session_->username(), session_->password(),
        session_->allocator()->origin(), emit_local_candidate_for_anyaddress,
        session_->allocator()->stun_candidate_keepalive_interval());
  } else {
    port = UDPPort::Create(
        session_->network_thread(), session_->socket_factory(), network_,
        session_->allocator()->min_port(), session_->allocator()->max_port(),
        session_->username(), session_->password(),
        session_->allocator()->origin(), emit_local_candidate_for_anyaddress,
        session_->allocator()->stun_candidate_keepalive_interval());
  }

  if (port) {
    // If shared socket is enabled, STUN candidate will be allocated by the
    // UDPPort.
    if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
      udp_port_ = port.get();
      port->SignalDestroyed.connect(this, &AllocationSequence::OnPortDestroyed);

      // If STUN is not disabled, setting stun server address to port.
      if (!IsFlagSet(PORTALLOCATOR_DISABLE_STUN)) {
        if (config_ && !config_->StunServers().empty()) {
          RTC_LOG(LS_INFO)
              << "AllocationSequence: UDPPort will be handling the "
                 "STUN candidate generation.";
          port->set_server_addresses(config_->StunServers());
        }
      }
    }

    session_->AddAllocatedPort(port.release(), this, true);
  }
}

void AllocationSequence::CreateTCPPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_TCP)) {
    RTC_LOG(LS_VERBOSE) << "AllocationSequence: TCP ports disabled, skipping.";
    return;
  }

  std::unique_ptr<Port> port = TCPPort::Create(
      session_->network_thread(), session_->socket_factory(), network_,
      session_->allocator()->min_port(), session_->allocator()->max_port(),
      session_->username(), session_->password(),
      session_->allocator()->allow_tcp_listen());
  if (port) {
    session_->AddAllocatedPort(port.release(), this, true);
    // Since TCPPort is not created using shared socket, |port| will not be
    // added to the dequeue.
  }
}

void AllocationSequence::CreateStunPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_STUN)) {
    RTC_LOG(LS_VERBOSE) << "AllocationSequence: STUN ports disabled, skipping.";
    return;
  }

  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
    return;
  }

  if (!(config_ && !config_->StunServers().empty())) {
    RTC_LOG(LS_WARNING)
        << "AllocationSequence: No STUN server configured, skipping.";
    return;
  }

  std::unique_ptr<StunPort> port = StunPort::Create(
      session_->network_thread(), session_->socket_factory(), network_,
      session_->allocator()->min_port(), session_->allocator()->max_port(),
      session_->username(), session_->password(), config_->StunServers(),
      session_->allocator()->origin(),
      session_->allocator()->stun_candidate_keepalive_interval());
  if (port) {
    session_->AddAllocatedPort(port.release(), this, true);
    // Since StunPort is not created using shared socket, |port| will not be
    // added to the dequeue.
  }
}

void AllocationSequence::CreateRelayPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_RELAY)) {
    RTC_LOG(LS_VERBOSE)
        << "AllocationSequence: Relay ports disabled, skipping.";
    return;
  }

  // If BasicPortAllocatorSession::OnAllocate left relay ports enabled then we
  // ought to have a relay list for them here.
  RTC_DCHECK(config_);
  RTC_DCHECK(!config_->relays.empty());
  if (!(config_ && !config_->relays.empty())) {
    RTC_LOG(LS_WARNING)
        << "AllocationSequence: No relay server configured, skipping.";
    return;
  }

  for (RelayServerConfig& relay : config_->relays) {
    CreateTurnPort(relay);
  }
}

void AllocationSequence::CreateTurnPort(const RelayServerConfig& config) {
  PortList::const_iterator relay_port;
  for (relay_port = config.ports.begin(); relay_port != config.ports.end();
       ++relay_port) {
    // Skip UDP connections to relay servers if it's disallowed.
    if (IsFlagSet(PORTALLOCATOR_DISABLE_UDP_RELAY) &&
        relay_port->proto == PROTO_UDP) {
      continue;
    }

    // Do not create a port if the server address family is known and does
    // not match the local IP address family.
    int server_ip_family = relay_port->address.ipaddr().family();
    int local_ip_family = network_->GetBestIP().family();
    if (server_ip_family != AF_UNSPEC && server_ip_family != local_ip_family) {
      RTC_LOG(LS_INFO)
          << "Server and local address families are not compatible. "
             "Server address: "
          << relay_port->address.ipaddr().ToSensitiveString()
          << " Local address: " << network_->GetBestIP().ToSensitiveString();
      continue;
    }

    CreateRelayPortArgs args;
    args.network_thread = session_->network_thread();
    args.socket_factory = session_->socket_factory();
    args.network = network_;
    args.username = session_->username();
    args.password = session_->password();
    args.server_address = &(*relay_port);
    args.config = &config;
    args.origin = session_->allocator()->origin();
    args.turn_customizer = session_->allocator()->turn_customizer();

    std::unique_ptr<cricket::Port> port;
    // Shared socket mode must be enabled only for UDP based ports. Hence
    // don't pass shared socket for ports which will create TCP sockets.
    // TODO(mallinath) - Enable shared socket mode for TURN ports. Disabled
    // due to webrtc bug https://code.google.com/p/webrtc/issues/detail?id=3537
    if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET) &&
        relay_port->proto == PROTO_UDP && udp_socket_) {
      port = session_->allocator()->relay_port_factory()->Create(
          args, udp_socket_.get());

      if (!port) {
        RTC_LOG(LS_WARNING) << "Failed to create relay port with "
                            << args.server_address->address.ToSensitiveString();
        continue;
      }

      relay_ports_.push_back(port.get());
      // Listen to the port destroyed signal, to allow AllocationSequence to
      // remove entrt from it's map.
      port->SignalDestroyed.connect(this, &AllocationSequence::OnPortDestroyed);
    } else {
      port = session_->allocator()->relay_port_factory()->Create(
          args, session_->allocator()->min_port(),
          session_->allocator()->max_port());

      if (!port) {
        RTC_LOG(LS_WARNING) << "Failed to create relay port with "
                            << args.server_address->address.ToSensitiveString();
        continue;
      }
    }
    RTC_DCHECK(port != NULL);
    session_->AddAllocatedPort(port.release(), this, true);
  }
}

void AllocationSequence::OnReadPacket(rtc::AsyncPacketSocket* socket,
                                      const char* data,
                                      size_t size,
                                      const rtc::SocketAddress& remote_addr,
                                      const int64_t& packet_time_us) {
  RTC_DCHECK(socket == udp_socket_.get());

  bool turn_port_found = false;

  // Try to find the TurnPort that matches the remote address. Note that the
  // message could be a STUN binding response if the TURN server is also used as
  // a STUN server. We don't want to parse every message here to check if it is
  // a STUN binding response, so we pass the message to TurnPort regardless of
  // the message type. The TurnPort will just ignore the message since it will
  // not find any request by transaction ID.
  for (auto* port : relay_ports_) {
    if (port->CanHandleIncomingPacketsFrom(remote_addr)) {
      if (port->HandleIncomingPacket(socket, data, size, remote_addr,
                                     packet_time_us)) {
        return;
      }
      turn_port_found = true;
    }
  }

  if (udp_port_) {
    const ServerAddresses& stun_servers = udp_port_->server_addresses();

    // Pass the packet to the UdpPort if there is no matching TurnPort, or if
    // the TURN server is also a STUN server.
    if (!turn_port_found ||
        stun_servers.find(remote_addr) != stun_servers.end()) {
      RTC_DCHECK(udp_port_->SharedSocket());
      udp_port_->HandleIncomingPacket(socket, data, size, remote_addr,
                                      packet_time_us);
    }
  }
}

void AllocationSequence::OnPortDestroyed(PortInterface* port) {
  if (udp_port_ == port) {
    udp_port_ = NULL;
    return;
  }

  auto it = absl::c_find(relay_ports_, port);
  if (it != relay_ports_.end()) {
    relay_ports_.erase(it);
  } else {
    RTC_LOG(LS_ERROR) << "Unexpected OnPortDestroyed for nonexistent port.";
    RTC_NOTREACHED();
  }
}
}  // namespace cricket
