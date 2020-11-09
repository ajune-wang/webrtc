/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_CLIENT_BASIC_PORT_ALLOCATION_CONFIGURATION_H_
#define P2P_CLIENT_BASIC_PORT_ALLOCATION_CONFIGURATION_H_

#include <memory>
#include <string>
#include <vector>

#include "api/turn_customizer.h"
#include "p2p/base/port_allocator.h"
#include "p2p/client/relay_port_factory_interface.h"
#include "p2p/client/turn_port_factory.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/checks.h"
#include "rtc_base/network.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread.h"

namespace cricket {
// Records configuration information useful in creating ports.
// TODO(deadbeef): Rename "relay" to "turn_server" in this struct.
struct RTC_EXPORT PortConfiguration : public rtc::MessageData {
  // TODO(jiayl): remove |stun_address| when Chrome is updated.
  rtc::SocketAddress stun_address;
  ServerAddresses stun_servers;
  std::string username;
  std::string password;
  bool use_turn_server_as_stun_server_disabled = false;

  typedef std::vector<RelayServerConfig> RelayList;
  RelayList relays;

  // TODO(jiayl): remove this ctor when Chrome is updated.
  PortConfiguration(const rtc::SocketAddress& stun_address,
                    const std::string& username,
                    const std::string& password);

  PortConfiguration(const ServerAddresses& stun_servers,
                    const std::string& username,
                    const std::string& password);

  ~PortConfiguration() override;

  // Returns addresses of both the explicitly configured STUN servers,
  // and TURN servers that should be used as STUN servers.
  ServerAddresses StunServers();

  // Adds another relay server, with the given ports and modifier, to the list.
  void AddRelay(const RelayServerConfig& config);

  // Determines whether the given relay server supports the given protocol.
  bool SupportsProtocol(const RelayServerConfig& relay,
                        ProtocolType type) const;
  bool SupportsProtocol(ProtocolType type) const;
  // Helper method returns the server addresses for the matching RelayType and
  // Protocol type.
  ServerAddresses GetRelayServerAddresses(ProtocolType type) const;
};

class UDPPort;
class TurnPort;
}  // namespace cricket

#endif  // P2P_CLIENT_BASIC_PORT_ALLOCATION_CONFIGURATION_H_
