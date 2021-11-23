/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <iostream>

#include "p2p/base/stun_server.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"

using cricket::StunServer;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "usage: stunserver address" << std::endl;
    return 1;
  }

  rtc::SocketAddress server_addr;
  if (!server_addr.FromString(argv[1])) {
    std::cerr << "Unable to parse IP address: " << argv[1];
    return 1;
  }

  rtc::PhysicalSocketServer socket_server;
  rtc::AutoSocketServerThread main_thread(&socket_server);

  rtc::AsyncUDPSocket* server_socket =
      rtc::AsyncUDPSocket::Create(&socket_server, server_addr);
  if (!server_socket) {
    std::cerr << "Failed to create a UDP socket" << std::endl;
    return 1;
  }

  auto server = std::make_unique<StunServer>(server_socket);

  std::cout << "Listening at " << server_addr.ToString() << std::endl;

  main_thread.Run();

  return 0;
}
