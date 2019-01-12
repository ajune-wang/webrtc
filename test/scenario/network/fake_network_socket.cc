/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/network/fake_network_socket.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "rtc_base/logging.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace test {

namespace {
std::string ToString(const rtc::SocketAddress& addr) {
  return addr.HostAsURIString() + ":" + std::to_string(addr.port());
}
}  // namespace

FakeNetworkSocket::FakeNetworkSocket(SocketManager* socket_manager)
    : socket_manager_(socket_manager),
      state_(CS_CLOSED),
      error_(0),
      pending_read_events_count_(0) {}
FakeNetworkSocket::~FakeNetworkSocket() {
  Close();
  socket_manager_->Unregister(this);
}

void FakeNetworkSocket::OnPacketReceived(EmulatedIpPacket packet) {
  {
    rtc::CritScope crit(&lock_);
    packet_queue_.push_back(std::move(packet));
    pending_read_events_count_++;
  }
  socket_manager_->WakeUp();
}

bool FakeNetworkSocket::HasIOToProcess() {
  rtc::CritScope crit(&lock_);
  // It is fine, if we will return true, but cuncurrent thread will remove
  // last event. In such case ProcessIO will just return immediately.
  return pending_read_events_count_ > 0;
}

void FakeNetworkSocket::ProcessIO() {
  {
    rtc::CritScope crit(&lock_);
    if (pending_read_events_count_ == 0) {
      return;
    }
    pending_read_events_count_--;
    RTC_DCHECK_GE(pending_read_events_count_, 0);
  }
  SignalReadEvent(this);
}

rtc::SocketAddress FakeNetworkSocket::GetLocalAddress() const {
  return local_addr_;
}

rtc::SocketAddress FakeNetworkSocket::GetRemoteAddress() const {
  return remote_addr_;
}

int FakeNetworkSocket::Bind(const rtc::SocketAddress& addr) {
  if (!local_addr_.IsNil()) {
    RTC_LOG(LS_ERROR) << "Socket already bound to address: "
                      << ToString(local_addr_);
    error_ = EINVAL;
    return -1;
  }
  local_addr_ = addr;
  endpoint_ = socket_manager_->GetEndpointNode(local_addr_.ipaddr());
  if (!endpoint_) {
    local_addr_.Clear();
    RTC_LOG(LS_ERROR) << "No endpoint for address: " << ToString(addr);
    error_ = EADDRNOTAVAIL;
    return 2;
  }
  absl::optional<uint16_t> port =
      endpoint_->BindReceiver(local_addr_.port(), this);
  if (!port) {
    local_addr_.Clear();
    RTC_LOG(LS_ERROR) << "Cannot bind to in-use address: " << ToString(addr);
    error_ = EADDRINUSE;
    return 1;
  }
  local_addr_.SetPort(port.value());
  return 0;
}

int FakeNetworkSocket::Connect(const rtc::SocketAddress& addr) {
  if (!remote_addr_.IsNil()) {
    RTC_LOG(LS_ERROR) << "Socket already connected to address: "
                      << ToString(remote_addr_);
    error_ = EISCONN;
    return -1;
  }
  if (local_addr_.IsNil()) {
    RTC_LOG(LS_ERROR) << "Socket have to be bind to some local address";
    error_ = EADDRNOTAVAIL;
    return 2;
  }
  remote_addr_ = addr;
  state_ = CS_CONNECTED;
  return 0;
}

int FakeNetworkSocket::Send(const void* pv, size_t cb) {
  if (state_ != CS_CONNECTED) {
    RTC_LOG(LS_ERROR) << "Socket cannot send: not connected";
    error_ = ENOTCONN;
    return -1;
  }
  return SendTo(pv, cb, remote_addr_);
}

int FakeNetworkSocket::SendTo(const void* pv,
                              size_t cb,
                              const rtc::SocketAddress& addr) {
  if (local_addr_.IsNil()) {
    RTC_LOG(LS_ERROR) << "Socket have to be bind to some local address";
    error_ = EADDRNOTAVAIL;
    return 2;
  }
  rtc::CopyOnWriteBuffer packet(static_cast<const uint8_t*>(pv), cb);
  endpoint_->SendPacket(local_addr_, addr, packet);
  return cb;
}

int FakeNetworkSocket::Recv(void* pv, size_t cb, int64_t* timestamp) {
  rtc::SocketAddress paddr;
  return RecvFrom(pv, cb, &paddr, timestamp);
}

int FakeNetworkSocket::RecvFrom(void* pv,
                                size_t cb,
                                rtc::SocketAddress* paddr,
                                int64_t* timestamp) {
  if (timestamp) {
    *timestamp = -1;
  }
  absl::optional<EmulatedIpPacket> packetOpt = PopFrontPacket();

  if (!packetOpt) {
    error_ = EAGAIN;
    return -1;
  }

  EmulatedIpPacket packet = std::move(packetOpt.value());
  *paddr = packet.from;
  size_t data_read = std::min(cb, packet.size());
  memcpy(pv, packet.cdata(), data_read);

  if (data_read < packet.data.size()) {
    packet.data = rtc::CopyOnWriteBuffer(packet.cdata() + data_read,
                                         packet.size() - data_read);
    {
      rtc::CritScope crit(&lock_);
      packet_queue_.push_front(std::move(packet));
    }
    SignalReadEvent(this);
  }

  return static_cast<int>(data_read);
}

int FakeNetworkSocket::Listen(int backlog) {
  RTC_CHECK(false) << "Listen() isn't valid for SOCK_DGRAM";
}

rtc::AsyncSocket* FakeNetworkSocket::Accept(rtc::SocketAddress* /*paddr*/) {
  RTC_CHECK(false) << "Accept() isn't valid for SOCK_DGRAM";
}

int FakeNetworkSocket::Close() {
  state_ = CS_CLOSED;
  if (!local_addr_.IsNil()) {
    endpoint_->UnbindReceiver(local_addr_.port());
  }
  local_addr_.Clear();
  remote_addr_.Clear();
  return 0;
}

int FakeNetworkSocket::GetError() const {
  RTC_CHECK(error_ == 0);
  return error_;
}

void FakeNetworkSocket::SetError(int error) {
  RTC_CHECK(error == 0);
  error_ = error;
}

rtc::AsyncSocket::ConnState FakeNetworkSocket::GetState() const {
  return state_;
}

int FakeNetworkSocket::GetOption(Option opt, int* value) {
  auto it = options_map_.find(opt);
  if (it == options_map_.end()) {
    return -1;
  }
  *value = it->second;
  return 0;
}

int FakeNetworkSocket::SetOption(Option opt, int value) {
  options_map_[opt] = value;
  return 0;
}

absl::optional<EmulatedIpPacket> FakeNetworkSocket::PopFrontPacket() {
  rtc::CritScope crit(&lock_);
  if (packet_queue_.empty()) {
    return absl::nullopt;
  }

  absl::optional<EmulatedIpPacket> packet =
      absl::make_optional(std::move(packet_queue_.front()));
  packet_queue_.pop_front();
  return packet;
}

}  // namespace test
}  // namespace webrtc
