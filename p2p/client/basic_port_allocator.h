/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_CLIENT_BASIC_PORT_ALLOCATOR_H_
#define P2P_CLIENT_BASIC_PORT_ALLOCATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "api/turn_customizer.h"
#include "p2p/base/port_allocator.h"
#include "p2p/client/basic_port_allocator_session.h"
#include "p2p/client/relay_port_factory_interface.h"
#include "p2p/client/turn_port_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/network.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread.h"

namespace cricket {

class RTC_EXPORT BasicPortAllocator : public PortAllocator {
 public:
  // note: The (optional) relay_port_factory is owned by caller
  // and must have a life time that exceeds that of BasicPortAllocator.
  BasicPortAllocator(rtc::NetworkManager* network_manager,
                     rtc::PacketSocketFactory* socket_factory,
                     webrtc::TurnCustomizer* customizer = nullptr,
                     RelayPortFactoryInterface* relay_port_factory = nullptr);
  explicit BasicPortAllocator(rtc::NetworkManager* network_manager);
  BasicPortAllocator(rtc::NetworkManager* network_manager,
                     const ServerAddresses& stun_servers);
  BasicPortAllocator(rtc::NetworkManager* network_manager,
                     rtc::PacketSocketFactory* socket_factory,
                     const ServerAddresses& stun_servers);
  ~BasicPortAllocator() override;

  // Set to kDefaultNetworkIgnoreMask by default.
  void SetNetworkIgnoreMask(int network_ignore_mask) override;
  int network_ignore_mask() const {
    CheckRunOnValidThreadIfInitialized();
    return network_ignore_mask_;
  }

  rtc::NetworkManager* network_manager() const {
    CheckRunOnValidThreadIfInitialized();
    return network_manager_;
  }

  // If socket_factory() is set to NULL each PortAllocatorSession
  // creates its own socket factory.
  rtc::PacketSocketFactory* socket_factory() {
    CheckRunOnValidThreadIfInitialized();
    return socket_factory_;
  }

  PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) override;

  // Convenience method that adds a TURN server to the configuration.
  void AddTurnServer(const RelayServerConfig& turn_server);

  RelayPortFactoryInterface* relay_port_factory() {
    CheckRunOnValidThreadIfInitialized();
    return relay_port_factory_;
  }

 private:
  void OnIceRegathering(PortAllocatorSession* session,
                        IceRegatheringReason reason);

  // This function makes sure that relay_port_factory_ is set properly.
  void InitRelayPortFactory(RelayPortFactoryInterface* relay_port_factory);

  bool MdnsObfuscationEnabled() const override;

  rtc::NetworkManager* network_manager_;
  rtc::PacketSocketFactory* socket_factory_;
  int network_ignore_mask_ = rtc::kDefaultNetworkIgnoreMask;

  // This is the factory being used.
  RelayPortFactoryInterface* relay_port_factory_;

  // This instance is created if caller does pass a factory.
  std::unique_ptr<RelayPortFactoryInterface> default_relay_port_factory_;
};


class UDPPort;
class TurnPort;

}  // namespace cricket
#endif  // P2P_CLIENT_BASIC_PORT_ALLOCATOR_H_
