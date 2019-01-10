/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_FAKE_NETWORK_SOCKET_H_
#define TEST_SCENARIO_NETWORK_FAKE_NETWORK_SOCKET_H_

#include <deque>
#include <map>
#include <vector>

#include "rtc_base/asyncsocket.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/socketaddress.h"
#include "test/scenario/network/network_emulation.h"

namespace webrtc {
namespace test {

// Provides endpoints by IP address
class EndpointProvider {
 public:
  virtual ~EndpointProvider() = default;

  virtual EndpointNode* GetEndpointNode(rtc::IPAddress ip) = 0;
};

// Represents a socket, which will operate with emulated network.
class FakeNetworkSocket : public rtc::AsyncSocket,
                          public EmulatedNetworkReceiverInterface {
 public:
  explicit FakeNetworkSocket(EndpointProvider* endpoint_provider);
  ~FakeNetworkSocket() override;

  // Will be invoked by EndpointNode to deliver packets into socket.
  void OnPacketReceived(EmulatedIpPacket packet) override;

  // rtc::Socket methods:
  rtc::SocketAddress GetLocalAddress() const override;
  rtc::SocketAddress GetRemoteAddress() const override;
  int Bind(const rtc::SocketAddress& addr) override;
  int Connect(const rtc::SocketAddress& addr) override;
  int Close() override;
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
  rtc::AsyncSocket* Accept(rtc::SocketAddress* paddr) override;
  int GetError() const override;
  void SetError(int error) override;
  ConnState GetState() const override;
  int GetOption(Option opt, int* value) override;
  int SetOption(Option opt, int value) override;

 private:
  EndpointProvider* const endpoint_provider_;
  EndpointNode* endpoint_;

  rtc::SocketAddress local_addr_;
  rtc::SocketAddress remote_addr_;
  ConnState state_;
  int error_;
  std::deque<EmulatedIpPacket> packet_queue_;
  std::map<Option, int> options_map_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_FAKE_NETWORK_SOCKET_H_
