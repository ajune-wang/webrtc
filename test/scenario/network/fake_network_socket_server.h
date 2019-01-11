/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_FAKE_NETWORK_SOCKET_SERVER_H_
#define TEST_SCENARIO_NETWORK_FAKE_NETWORK_SOCKET_SERVER_H_

#include <vector>

#include "api/units/timestamp.h"
#include "rtc_base/asyncsocket.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/messagequeue.h"
#include "rtc_base/socket.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/socketserver.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "system_wrappers/include/clock.h"
#include "test/scenario/network/fake_network_socket.h"

namespace webrtc {
namespace test {

// FakeNetworkSocketServer must outlive any sockets it creates.
class FakeNetworkSocketServer : public rtc::SocketServer,
                                public sigslot::has_slots<>,
                                public SocketManager {
 public:
  FakeNetworkSocketServer(Clock* clock, std::vector<EndpointNode*> endpoints);
  ~FakeNetworkSocketServer() override;

  EndpointNode* GetEndpointNode(const rtc::IPAddress& ip) override;
  void Unregister(SocketIOProcessor* io_processor) override;
  void OnMessageQueueDestroyed();

  // rtc::SocketFactory methods:
  rtc::Socket* CreateSocket(int family, int type) override;
  rtc::AsyncSocket* CreateAsyncSocket(int family, int type) override;

  // rtc::SocketServer methods:
  // Called by the network thread when this server is installed, kicking off the
  // message handler loop.
  void SetMessageQueue(rtc::MessageQueue* msg_queue) override;
  bool Wait(int cms, bool process_io) override;
  void WakeUp() override;

 private:
  Timestamp Now() const;

  Clock* const clock_;
  const std::vector<EndpointNode*> endpoints_;
  rtc::Event wakeup_;
  rtc::MessageQueue* msg_queue_;

  rtc::CriticalSection lock_;
  std::set<SocketIOProcessor*> io_processors_ RTC_GUARDED_BY(lock_);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_FAKE_NETWORK_SOCKET_SERVER_H_
