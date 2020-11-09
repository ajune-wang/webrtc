/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/client/basic_port_allocation_configuration.h"

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

namespace cricket {
namespace {
// PortConfiguration
PortConfiguration::PortConfiguration(const rtc::SocketAddress& stun_address,
                                     const std::string& username,
                                     const std::string& password)
    : stun_address(stun_address), username(username), password(password) {
  if (!stun_address.IsNil())
    stun_servers.insert(stun_address);
}

PortConfiguration::PortConfiguration(const ServerAddresses& stun_servers,
                                     const std::string& username,
                                     const std::string& password)
    : stun_servers(stun_servers), username(username), password(password) {
  if (!stun_servers.empty())
    stun_address = *(stun_servers.begin());
  // Note that this won't change once the config is initialized.
  use_turn_server_as_stun_server_disabled =
      webrtc::field_trial::IsDisabled("WebRTC-UseTurnServerAsStunServer");
}

PortConfiguration::~PortConfiguration() = default;

ServerAddresses PortConfiguration::StunServers() {
  if (!stun_address.IsNil() &&
      stun_servers.find(stun_address) == stun_servers.end()) {
    stun_servers.insert(stun_address);
  }

  if (!stun_servers.empty() && use_turn_server_as_stun_server_disabled) {
    return stun_servers;
  }

  // Every UDP TURN server should also be used as a STUN server if
  // use_turn_server_as_stun_server is not disabled or the stun servers are
  // empty.
  ServerAddresses turn_servers = GetRelayServerAddresses(PROTO_UDP);
  for (const rtc::SocketAddress& turn_server : turn_servers) {
    if (stun_servers.find(turn_server) == stun_servers.end()) {
      stun_servers.insert(turn_server);
    }
  }
  return stun_servers;
}

void PortConfiguration::AddRelay(const RelayServerConfig& config) {
  relays.push_back(config);
}

bool PortConfiguration::SupportsProtocol(const RelayServerConfig& relay,
                                         ProtocolType type) const {
  PortList::const_iterator relay_port;
  for (relay_port = relay.ports.begin(); relay_port != relay.ports.end();
       ++relay_port) {
    if (relay_port->proto == type)
      return true;
  }
  return false;
}

bool PortConfiguration::SupportsProtocol(ProtocolType type) const {
  for (size_t i = 0; i < relays.size(); ++i) {
    if (SupportsProtocol(relays[i], type))
      return true;
  }
  return false;
}

ServerAddresses PortConfiguration::GetRelayServerAddresses(
    ProtocolType type) const {
  ServerAddresses servers;
  for (size_t i = 0; i < relays.size(); ++i) {
    if (SupportsProtocol(relays[i], type)) {
      servers.insert(relays[i].ports.front().address);
    }
  }
  return servers;
}
}  // namespace
}  // namespace cricket