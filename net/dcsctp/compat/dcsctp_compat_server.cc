/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/compat/dcsctp_compat_server.h"

#include "absl/algorithm/container.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_tcp_socket.h"
#include "rtc_base/logging.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"

namespace dcsctp {
namespace compat {
namespace {
rtc::AsyncPacketSocket* CreateSocket(rtc::Thread* thread,
                                     const rtc::SocketAddress& addr) {
  rtc::AsyncSocket* socket =
      thread->socketserver()->CreateAsyncSocket(addr.family(), SOCK_STREAM);
  socket->Bind(addr);

  return new rtc::AsyncTCPSocket(socket, true);
}
}  // namespace

DcsctpCompatServer::DcsctpCompatServer(rtc::Thread* thread,
                                       const rtc::SocketAddress& addr)
    : server_socket_(CreateSocket(thread, addr)) {
  server_socket_->SignalNewConnection.connect(
      this, &DcsctpCompatServer::OnConnection);
}

DcsctpCompatServer::~DcsctpCompatServer() {
  for (auto& c : clients_) {
    delete c.socket;
  }
}

void DcsctpCompatServer::OnConnection(rtc::AsyncPacketSocket* socket,
                                      rtc::AsyncPacketSocket* new_socket) {
  Incoming incoming;
  incoming.addr = new_socket->GetRemoteAddress();
  incoming.socket = new_socket;

  incoming.socket->SignalClose.connect(this, &DcsctpCompatServer::OnClose);
  incoming.socket->SignalReadPacket.connect(this,
                                            &DcsctpCompatServer::OnPacket);

  RTC_LOG(LS_VERBOSE) << ": Accepted connection from "
                      << incoming.addr.ToSensitiveString();
  clients_.push_back(incoming);

  incoming.socket->Send("Hello", 5, {});
}

void DcsctpCompatServer::OnPacket(rtc::AsyncPacketSocket* socket,
                                  const char* buf,
                                  size_t size,
                                  const rtc::SocketAddress& remote_addr,
                                  const int64_t& /* packet_time_us */) {
  RTC_LOG(LS_INFO) << "Received packet";
}

void DcsctpCompatServer::OnClose(rtc::AsyncPacketSocket* socket, int err) {
  RTC_LOG(LS_INFO) << "OnClose";
  rtc::Thread::Current()->Dispose(socket);
}

}  // namespace compat
}  // namespace dcsctp
