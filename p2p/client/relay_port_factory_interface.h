/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_CLIENT_RELAY_PORT_FACTORY_INTERFACE_H_
#define P2P_CLIENT_RELAY_PORT_FACTORY_INTERFACE_H_

#include <memory>
#include <string>

#include "p2p/base/port_interface.h"
#include "rtc_base/ref_count.h"

namespace rtc {
class AsyncPacketSocket;
class Network;
class PacketSocketFactory;
class Thread;
}  // namespace rtc

namespace webrtc {
class TurnCustomizer;
}  // namespace webrtc

namespace cricket {
class Port;
struct ProtocolAddress;
struct RelayServerConfig;

// A struct containing arguments to RelayPortFactory::Create()
struct CreateRelayPortArgs {
  CreateRelayPortArgs();
  rtc::Thread* network_thread = nullptr;
  rtc::PacketSocketFactory* socket_factory = nullptr;
  rtc::Network* network = nullptr;
  rtc::AsyncPacketSocket* shared_socket = nullptr;
  uint16_t min_port;
  uint16_t max_port;
  const ProtocolAddress* server_address = nullptr;
  const RelayServerConfig* config = nullptr;
  std::string username;
  std::string password;
  std::string origin;
  webrtc::TurnCustomizer* turn_customizer = nullptr;
};

inline CreateRelayPortArgs::CreateRelayPortArgs() {}

// A factory for creating RelayPort's.
class RelayPortFactoryInterface {
 public:
  virtual ~RelayPortFactoryInterface() {}

  // This variant is used for UDP connection to the relay server
  // using a already existing shared socket.
  virtual std::unique_ptr<Port> Create(const CreateRelayPortArgs& args,
                                       rtc::AsyncPacketSocket* udp_socket) = 0;

  // This variant is used for the other cases.
  virtual std::unique_ptr<Port> Create(const CreateRelayPortArgs& args,
                                       int min_port,
                                       int max_port) = 0;

  // TODO(qingsi): Remove the above two overloads once the downstream projects
  // have migrated to this one.
  virtual std::unique_ptr<Port> Create(const CreateRelayPortArgs& args) {
    if (args.shared_socket) {
      return Create(args, args.shared_socket);
    }

    return Create(args, args.min_port, args.max_port);
  }
};

}  // namespace cricket

#endif  // P2P_CLIENT_RELAY_PORT_FACTORY_INTERFACE_H_
