/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/udp_multiplex_socket.h"

#include <algorithm>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace cricket {

// copied from p2p/base/udp_transport.cc
constexpr size_t kMaxDtlsPacketLen = 2048;

class MultiplexedSocket : public rtc::Socket, public sigslot::has_slots<> {
 public:
  explicit MultiplexedSocket(UdpMultiplexSocket* socket,
                             const rtc::SocketAddress& remote_addr)
      : socket_(socket), remote_addr_(remote_addr) {}

  ~MultiplexedSocket() override = default;

  rtc::SocketAddress GetLocalAddress() const override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return socket_->GetLocalAddress();
  }

  rtc::SocketAddress GetRemoteAddress() const override { return remote_addr_; }

  int Bind(const rtc::SocketAddress& addr) override {
    RTC_CHECK(false) << "Not implemented";
  }

  int Connect(const rtc::SocketAddress& addr) override {
    RTC_CHECK(false) << "Not implemented";
  }

  int Send(const void* pv, size_t cb) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    if (closed_) {
      SetError(EBADF);
      return -1;
    }
    return socket_->SendTo(pv, cb, remote_addr_);
  }

  int SendTo(const void* pv,
             size_t cb,
             const rtc::SocketAddress& addr) override {
    RTC_CHECK(false) << "Not implemented";
  }

  int Recv(void* pv, size_t cb, int64_t* timestamp) override {
    return RecvFrom(pv, cb, nullptr, timestamp);
  }

  int RecvFrom(void* pv,
               size_t cb,
               rtc::SocketAddress* paddr,
               int64_t* timestamp) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    if (closed_) {
      SetError(EBADF);
      return -1;
    }
    int result = socket_->ReadFromBuffer(pv, cb);
    if (result < 0) {
      SetError(socket_->GetError());
      socket_->SetError(0);
    } else {
      SetError(0);
    }
    if (result >= 0) {
      if (paddr != nullptr) {
        *paddr = remote_addr_;
      }
      if (timestamp != nullptr) {
        *timestamp = -1;
      }
    }
    return result;
  }

  int Listen(int backlog) override { RTC_CHECK(false) << "Not implemented"; }

  rtc::Socket* Accept(rtc::SocketAddress* paddr) override {
    RTC_CHECK(false) << "Not implemented";
  }

  int Close() override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    closed_ = true;
    return 0;
  }

  int GetError() const override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return error_;
  }

  void SetError(int error) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    error_ = error;
  }

  ConnState GetState() const override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return closed_ ? CS_CLOSED : CS_CONNECTED;
  }

  int GetOption(Option opt, int* value) override {
    RTC_CHECK(false) << "Not implemented";
  }

  int SetOption(Option opt, int value) override {
    RTC_CHECK(false) << "Not implemented";
  }

 private:
  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker sequence_checker_;

  UdpMultiplexSocket* socket_;
  const rtc::SocketAddress remote_addr_;
  bool closed_ RTC_GUARDED_BY(&sequence_checker_) = false;
  int error_ RTC_GUARDED_BY(&sequence_checker_) = 0;
};

UdpMultiplexSocket::UdpMultiplexSocket(rtc::Socket* socket) : socket_(socket) {
  RTC_CHECK(socket_);
  socket->SignalReadEvent.connect(this, &UdpMultiplexSocket::OnReadEvent);
}

rtc::SocketAddress UdpMultiplexSocket::GetLocalAddress() const {
  return socket_->GetLocalAddress();
}

rtc::SocketAddress UdpMultiplexSocket::GetRemoteAddress() const {
  RTC_CHECK(false) << "Not implemented";
}

int UdpMultiplexSocket::Bind(const rtc::SocketAddress& addr) {
  RTC_CHECK(false) << "Not implemented";
}

int UdpMultiplexSocket::Connect(const rtc::SocketAddress& addr) {
  RTC_CHECK(false) << "Not implemented";
}

int UdpMultiplexSocket::Send(const void* pv, size_t cb) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return socket_->Send(pv, cb);
}

int UdpMultiplexSocket::SendTo(const void* pv,
                               size_t cb,
                               const rtc::SocketAddress& addr) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return socket_->SendTo(pv, cb, addr);
}

int UdpMultiplexSocket::Recv(void* pv, size_t cb, int64_t* timestamp) {
  RTC_CHECK(false) << "Not implemented";
}

int UdpMultiplexSocket::RecvFrom(void* pv,
                                 size_t cb,
                                 rtc::SocketAddress* paddr,
                                 int64_t* timestamp) {
  RTC_CHECK(false) << "Not implemented";
}

int UdpMultiplexSocket::Listen(int backlog) {
  RTC_CHECK(false) << "Not implemented";
}

rtc::Socket* UdpMultiplexSocket::Accept(rtc::SocketAddress* paddr) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (pending_remote_addr_.has_value()) {
    *paddr = pending_remote_addr_.value();
    pending_remote_addr_ = absl::nullopt;
    rtc::Socket* socket = new MultiplexedSocket(this, *paddr);
    child_sockets_.insert(std::make_pair(*paddr, socket));
    return socket;
  }
  return nullptr;
}

int UdpMultiplexSocket::Close() {
  RTC_CHECK(false) << "Not implemented";
}

int UdpMultiplexSocket::GetError() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return error_;
}

void UdpMultiplexSocket::SetError(int error) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  error_ = error;
}

rtc::Socket::ConnState UdpMultiplexSocket::GetState() const {
  RTC_CHECK(false) << "Not implemented";
}

int UdpMultiplexSocket::GetOption(Option opt, int* value) {
  RTC_CHECK(false) << "Not implemented";
}

int UdpMultiplexSocket::SetOption(Option opt, int value) {
  RTC_CHECK(false) << "Not implemented";
}

void UdpMultiplexSocket::OnReadEvent(rtc::Socket* socket) {
  RTC_DCHECK(socket_.get() == socket);
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  read_buffer_.SetSize(kMaxDtlsPacketLen);
  rtc::SocketAddress remote_addr;
  int len = socket_->RecvFrom(read_buffer_.data(), read_buffer_.size(),
                              &remote_addr, nullptr);
  if (len < 0) {
    RTC_LOG(LS_INFO) << "UdpMultiplexSocket["
                     << socket_->GetLocalAddress().ToSensitiveString()
                     << "] receive failed with error " << socket_->GetError();
    return;
  }
  read_buffer_.SetSize(len);

  auto it = child_sockets_.find(remote_addr);
  if (it == child_sockets_.end()) {
    pending_remote_addr_ = remote_addr;
    SignalReadEvent(this);
    // Callbacks must read all data from the socket.
    RTC_DCHECK_EQ(read_buffer_.size(), 0u);
    return;
  }
  it->second->SignalReadEvent(it->second);
  // Callbacks must read all data from the socket.
  RTC_DCHECK_EQ(read_buffer_.size(), 0u);
}

int UdpMultiplexSocket::ReadFromBuffer(void* pv, size_t cb) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!read_buffer_.empty()) {
    size_t size = std::min(read_buffer_.size(), cb);
    std::memcpy(pv, read_buffer_.data(), size);
    read_buffer_.Clear();
    SetError(0);
    return size;
  }
  SetError(EWOULDBLOCK);
  return -1;
}

}  // namespace cricket
