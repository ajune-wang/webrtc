/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_DEFAULT_PORT_FACTORY_H_
#define P2P_BASE_DEFAULT_PORT_FACTORY_H_

#include "p2p/base/port_factory_interface.h"
#include "rtc_base/async_packet_socket.h"

namespace cricket {

class DefaultPortFactory : public PortFactoryInterface {
 public:
  DefaultPortFactory() = default;
  ~DefaultPortFactory() override {}

  std::unique_ptr<PortInterface> CreateRelayPort(
      const CreateRelayPortArgs& args,
      rtc::AsyncPacketSocket* udp_socket) override;

  std::unique_ptr<PortInterface> CreateRelayPort(
      const CreateRelayPortArgs& args,
      int min_port,
      int max_port) override;
};

}  // namespace cricket

#endif  // P2P_BASE_DEFAULT_PORT_FACTORY_H_
