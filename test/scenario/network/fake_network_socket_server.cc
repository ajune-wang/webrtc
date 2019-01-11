/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/network/fake_network_socket_server.h"

#include <utility>

namespace webrtc {
namespace test {

FakeNetworkSocketServer::FakeNetworkSocketServer(
    Clock* clock,
    std::vector<EndpointNode*> endpoints)
    : clock_(clock),
      endpoints_(std::move(endpoints)),
      wakeup_(/*manual_reset=*/false, /*initially_signaled=*/false) {}
FakeNetworkSocketServer::~FakeNetworkSocketServer() = default;

void FakeNetworkSocketServer::OnMessageQueueDestroyed() {
  msg_queue_ = nullptr;
}

EndpointNode* FakeNetworkSocketServer::GetEndpointNode(
    const rtc::IPAddress& ip) {
  for (auto* endpoint : endpoints_) {
    rtc::IPAddress peerLocalAddress = endpoint->GetPeerLocalAddress();
    if (peerLocalAddress == ip) {
      return endpoint;
    }
  }
  RTC_CHECK(false) << "No network found for address" << ip.ToString();
}

void FakeNetworkSocketServer::Unregister(SocketIOProcessor* io_processor) {
  rtc::CritScope crit(&lock_);
  io_processors_.erase(io_processor);
}

rtc::Socket* FakeNetworkSocketServer::CreateSocket(int /*family*/,
                                                   int /*type*/) {
  RTC_CHECK(false) << "Only async sockets are supported";
}

rtc::AsyncSocket* FakeNetworkSocketServer::CreateAsyncSocket(int family,
                                                             int type) {
  RTC_DCHECK(family == AF_INET || family == AF_INET6);
  // We support only UDP sockets for now.
  RTC_DCHECK(type == SOCK_DGRAM);
  FakeNetworkSocket* out = new FakeNetworkSocket(this);
  {
    rtc::CritScope crit(&lock_);
    io_processors_.insert(out);
  }
  return out;
}

void FakeNetworkSocketServer::SetMessageQueue(rtc::MessageQueue* msg_queue) {
  msg_queue_ = msg_queue;
  if (msg_queue_) {
    msg_queue_->SignalQueueDestroyed.connect(
        this, &FakeNetworkSocketServer::OnMessageQueueDestroyed);
  }
}

// Returns true on the timeout and false otherwise.
bool FakeNetworkSocketServer::Wait(int cms, bool process_io) {
  RTC_DCHECK(msg_queue_ == rtc::Thread::Current());
  // Note: we don't need to do anything with |process_io| since we don't have
  // any real I/O. Received packets come in the form of queued messages, so
  // MessageQueue will ensure WakeUp is called if another thread sends a
  // packet.
  while (true) {
    Timestamp start = Now();
    if (process_io) {
      rtc::CritScope crit(&lock_);
      for (auto* io_processor : io_processors_) {
        io_processor->ProcessIO();
      }
    }
    Timestamp end = Now();

    cms -= (end - start).ms();
    wakeup_.Wait(cms);
    if ((Now() - end).ms() >= cms) {
      break;
    }
  }
  return true;
}

void FakeNetworkSocketServer::WakeUp() {
  wakeup_.Set();
}

Timestamp FakeNetworkSocketServer::Now() const {
  return Timestamp::us(clock_->TimeInMicroseconds());
}

}  // namespace test
}  // namespace webrtc
