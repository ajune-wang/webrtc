/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_UDP_MULTIPLEX_SOCKET_H_
#define P2P_BASE_UDP_MULTIPLEX_SOCKET_H_

#include <map>
#include <memory>

#include "absl/types/optional.h"
#include "api/sequence_checker.h"
#include "rtc_base/buffer.h"
#include "rtc_base/socket.h"
#include "rtc_base/system/no_unique_address.h"

namespace cricket {

/**
 * UdpMultiplexSocket is a wrapper around a socket that allows it to be used
 * as if it were a TCP socket. The role of this class is give a child socket
 * on Accept() that can be used to communicate with the remote peer. Currently,
 * it is only used in tests.
 */
class UdpMultiplexSocket : public rtc::Socket, public sigslot::has_slots<> {
 public:
  explicit UdpMultiplexSocket(rtc::Socket* socket);

  ~UdpMultiplexSocket() override = default;

  rtc::SocketAddress GetLocalAddress() const override;

  rtc::SocketAddress GetRemoteAddress() const override;

  int Bind(const rtc::SocketAddress& addr) override;

  int Connect(const rtc::SocketAddress& addr) override;

  int Send(const void* pv, size_t cb) override;

  int SendTo(const void* pv,
             size_t cb,
             const rtc::SocketAddress& addr) override;

  int Recv(void* pv, size_t cb, int64_t* timestamp) override;

  int RecvFrom(void* pv,
               size_t cb,
               rtc::SocketAddress* paddr,
               int64_t* timestamp) override;

  int Listen(int backlog) override;

  rtc::Socket* Accept(rtc::SocketAddress* paddr) override;

  int Close() override;

  int GetError() const override;

  void SetError(int error) override;

  ConnState GetState() const override;

  int GetOption(Option opt, int* value) override;

  int SetOption(Option opt, int value) override;

 private:
  friend class MultiplexedSocket;

  void OnReadEvent(rtc::Socket* socket);

  int ReadFromBuffer(void* pv, size_t cb);

  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker sequence_checker_;

  std::unique_ptr<rtc::Socket> socket_;
  int error_ RTC_GUARDED_BY(sequence_checker_) = 0;
  rtc::Buffer read_buffer_ RTC_GUARDED_BY(sequence_checker_);

  absl::optional<rtc::SocketAddress> pending_remote_addr_
      RTC_GUARDED_BY(sequence_checker_);
  std::map<rtc::SocketAddress, rtc::Socket*> child_sockets_
      RTC_GUARDED_BY(sequence_checker_);
};

}  // namespace cricket

#endif  // P2P_BASE_UDP_MULTIPLEX_SOCKET_H_
