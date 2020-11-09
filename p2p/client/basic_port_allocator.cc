/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/client/basic_port_allocator.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/port.h"
#include "p2p/base/stun_port.h"
#include "p2p/base/tcp_port.h"
#include "p2p/base/turn_port.h"
#include "p2p/base/udp_port.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

using rtc::CreateRandomId;

namespace cricket {
namespace {

enum {
  MSG_CONFIG_START,
  MSG_CONFIG_READY,
  MSG_ALLOCATE,
  MSG_ALLOCATION_PHASE,
  MSG_SEQUENCEOBJECTS_CREATED,
  MSG_CONFIG_STOP,
};

const int PHASE_UDP = 0;
const int PHASE_RELAY = 1;
const int PHASE_TCP = 2;

const int kNumPhases = 3;

// Gets protocol priority: UDP > TCP > SSLTCP == TLS.
int GetProtocolPriority(cricket::ProtocolType protocol) {
  switch (protocol) {
    case cricket::PROTO_UDP:
      return 2;
    case cricket::PROTO_TCP:
      return 1;
    case cricket::PROTO_SSLTCP:
    case cricket::PROTO_TLS:
      return 0;
    default:
      RTC_NOTREACHED();
      return 0;
  }
}
// Gets address family priority:  IPv6 > IPv4 > Unspecified.
int GetAddressFamilyPriority(int ip_family) {
  switch (ip_family) {
    case AF_INET6:
      return 2;
    case AF_INET:
      return 1;
    default:
      RTC_NOTREACHED();
      return 0;
  }
}

// Returns positive if a is better, negative if b is better, and 0 otherwise.
int ComparePort(const cricket::Port* a, const cricket::Port* b) {
  int a_protocol = GetProtocolPriority(a->GetProtocol());
  int b_protocol = GetProtocolPriority(b->GetProtocol());
  int cmp_protocol = a_protocol - b_protocol;
  if (cmp_protocol != 0) {
    return cmp_protocol;
  }

  int a_family = GetAddressFamilyPriority(a->Network()->GetBestIP().family());
  int b_family = GetAddressFamilyPriority(b->Network()->GetBestIP().family());
  return a_family - b_family;
}

struct NetworkFilter {
  using Predicate = std::function<bool(rtc::Network*)>;
  NetworkFilter(Predicate pred, const std::string& description)
      : predRemain([pred](rtc::Network* network) { return !pred(network); }),
        description(description) {}
  Predicate predRemain;
  const std::string description;
};

using NetworkList = rtc::NetworkManager::NetworkList;
void FilterNetworks(NetworkList* networks, NetworkFilter filter) {
  auto start_to_remove =
      std::partition(networks->begin(), networks->end(), filter.predRemain);
  if (start_to_remove == networks->end()) {
    return;
  }
  RTC_LOG(INFO) << "Filtered out " << filter.description << " networks:";
  for (auto it = start_to_remove; it != networks->end(); ++it) {
    RTC_LOG(INFO) << (*it)->ToString();
  }
  networks->erase(start_to_remove, networks->end());
}

bool IsAllowedByCandidateFilter(const Candidate& c, uint32_t filter) {
  // When binding to any address, before sending packets out, the getsockname
  // returns all 0s, but after sending packets, it'll be the NIC used to
  // send. All 0s is not a valid ICE candidate address and should be filtered
  // out.
  if (c.address().IsAnyIP()) {
    return false;
  }

  if (c.type() == RELAY_PORT_TYPE) {
    return ((filter & CF_RELAY) != 0);
  } else if (c.type() == STUN_PORT_TYPE) {
    return ((filter & CF_REFLEXIVE) != 0);
  } else if (c.type() == LOCAL_PORT_TYPE) {
    if ((filter & CF_REFLEXIVE) && !c.address().IsPrivateIP()) {
      // We allow host candidates if the filter allows server-reflexive
      // candidates and the candidate is a public IP. Because we don't generate
      // server-reflexive candidates if they have the same IP as the host
      // candidate (i.e. when the host candidate is a public IP), filtering to
      // only server-reflexive candidates won't work right when the host
      // candidates have public IPs.
      return true;
    }

    return ((filter & CF_HOST) != 0);
  }
  return false;
}

}  // namespace

const uint32_t DISABLE_ALL_PHASES =
    PORTALLOCATOR_DISABLE_UDP | PORTALLOCATOR_DISABLE_TCP |
    PORTALLOCATOR_DISABLE_STUN | PORTALLOCATOR_DISABLE_RELAY;

// BasicPortAllocator
BasicPortAllocator::BasicPortAllocator(
    rtc::NetworkManager* network_manager,
    rtc::PacketSocketFactory* socket_factory,
    webrtc::TurnCustomizer* customizer,
    RelayPortFactoryInterface* relay_port_factory)
    : network_manager_(network_manager), socket_factory_(socket_factory) {
  InitRelayPortFactory(relay_port_factory);
  RTC_DCHECK(relay_port_factory_ != nullptr);
  RTC_DCHECK(network_manager_ != nullptr);
  RTC_DCHECK(socket_factory_ != nullptr);
  SetConfiguration(ServerAddresses(), std::vector<RelayServerConfig>(), 0,
                   webrtc::NO_PRUNE, customizer);
}

BasicPortAllocator::BasicPortAllocator(rtc::NetworkManager* network_manager)
    : network_manager_(network_manager), socket_factory_(nullptr) {
  InitRelayPortFactory(nullptr);
  RTC_DCHECK(relay_port_factory_ != nullptr);
  RTC_DCHECK(network_manager_ != nullptr);
}

BasicPortAllocator::BasicPortAllocator(rtc::NetworkManager* network_manager,
                                       const ServerAddresses& stun_servers)
    : BasicPortAllocator(network_manager,
                         /*socket_factory=*/nullptr,
                         stun_servers) {}

BasicPortAllocator::BasicPortAllocator(rtc::NetworkManager* network_manager,
                                       rtc::PacketSocketFactory* socket_factory,
                                       const ServerAddresses& stun_servers)
    : network_manager_(network_manager), socket_factory_(socket_factory) {
  InitRelayPortFactory(nullptr);
  RTC_DCHECK(relay_port_factory_ != nullptr);
  SetConfiguration(stun_servers, std::vector<RelayServerConfig>(), 0,
                   webrtc::NO_PRUNE, nullptr);
}

void BasicPortAllocator::OnIceRegathering(PortAllocatorSession* session,
                                          IceRegatheringReason reason) {
  // If the session has not been taken by an active channel, do not report the
  // metric.
  for (auto& allocator_session : pooled_sessions()) {
    if (allocator_session.get() == session) {
      return;
    }
  }

  RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.IceRegatheringReason",
                            static_cast<int>(reason),
                            static_cast<int>(IceRegatheringReason::MAX_VALUE));
}

BasicPortAllocator::~BasicPortAllocator() {
  CheckRunOnValidThreadIfInitialized();
  // Our created port allocator sessions depend on us, so destroy our remaining
  // pooled sessions before anything else.
  DiscardCandidatePool();
}

void BasicPortAllocator::SetNetworkIgnoreMask(int network_ignore_mask) {
  // TODO(phoglund): implement support for other types than loopback.
  // See https://code.google.com/p/webrtc/issues/detail?id=4288.
  // Then remove set_network_ignore_list from NetworkManager.
  CheckRunOnValidThreadIfInitialized();
  network_ignore_mask_ = network_ignore_mask;
}

PortAllocatorSession* BasicPortAllocator::CreateSessionInternal(
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag,
    const std::string& ice_pwd) {
  CheckRunOnValidThreadAndInitialized();
  PortAllocatorSession* session = new BasicPortAllocatorSession(
      this, content_name, component, ice_ufrag, ice_pwd);
  session->SignalIceRegathering.connect(this,
                                        &BasicPortAllocator::OnIceRegathering);
  return session;
}

void BasicPortAllocator::AddTurnServer(const RelayServerConfig& turn_server) {
  CheckRunOnValidThreadAndInitialized();
  std::vector<RelayServerConfig> new_turn_servers = turn_servers();
  new_turn_servers.push_back(turn_server);
  SetConfiguration(stun_servers(), new_turn_servers, candidate_pool_size(),
                   turn_port_prune_policy(), turn_customizer());
}

void BasicPortAllocator::InitRelayPortFactory(
    RelayPortFactoryInterface* relay_port_factory) {
  if (relay_port_factory != nullptr) {
    relay_port_factory_ = relay_port_factory;
  } else {
    default_relay_port_factory_.reset(new TurnPortFactory());
    relay_port_factory_ = default_relay_port_factory_.get();
  }
}

bool BasicPortAllocator::MdnsObfuscationEnabled() const {
  return network_manager()->GetMdnsResponder() != nullptr;
}

}  // namespace cricket