/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_COMPAT_DCSCTP_COMPAT_SERVER_H_
#define NET_DCSCTP_COMPAT_DCSCTP_COMPAT_SERVER_H_

#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_tcp_socket.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace dcsctp {
namespace compat {

class DcsctpCompatServer : public sigslot::has_slots<> {
 public:
  DcsctpCompatServer(rtc::Thread* thread, const rtc::SocketAddress& addr);
  ~DcsctpCompatServer() override;

  rtc::SocketAddress address() const {
    return server_socket_->GetLocalAddress();
  }

 private:
  struct Incoming {
    rtc::SocketAddress addr;
    rtc::AsyncPacketSocket* socket;
  };
  void OnConnection(rtc::AsyncPacketSocket* socket,
                    rtc::AsyncPacketSocket* new_socket);
  void OnPacket(rtc::AsyncPacketSocket* socket,
                const char* buf,
                size_t size,
                const rtc::SocketAddress& remote_addr,
                const int64_t& /* packet_time_us */);
  void OnClose(rtc::AsyncPacketSocket* socket, int err);
  void OnConnect(rtc::AsyncPacketSocket* s);

  std::unique_ptr<rtc::AsyncPacketSocket> server_socket_;
  std::vector<Incoming> clients_;
};

}  // namespace compat
}  // namespace dcsctp

#endif  // NET_DCSCTP_COMPAT_DCSCTP_COMPAT_SERVER_H_
