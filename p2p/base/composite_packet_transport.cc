/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "p2p/base/composite_packet_transport.h"

#include "rtc_base/checks.h"

namespace rtc {

CompositePacketTransport::CompositePacketTransport(
    std::vector<PacketTransportInternal*> transports)
    : transports_(std::move(transports)) {
  RTC_DCHECK(!transports_.empty());
  for (auto transport : transports_) {
    RTC_DCHECK(transport->transport_name() == transport_name());

    // Forward receive-oriented signals to the upper layer.
    // Note that SignalWritableState, SignalReadyToSend, and SignalSentPacket
    // are *not* forwarded, as the composite itself does not become writable or
    // able to send.
    transport->SignalReceivingState.connect(
        this, &CompositePacketTransport::OnReceivingState);
    transport->SignalReadPacket.connect(
        this, &CompositePacketTransport::OnReadPacket);
    transport->SignalNetworkRouteChanged.connect(
        this, &CompositePacketTransport::OnNetworkRouteChanged);
  }
}

bool CompositePacketTransport::SetSendTransport(
    PacketTransportInternal* send_transport) {
  if (send_transport_ == send_transport) {
    return true;
  }
  auto it = std::find(transports_.begin(), transports_.end(), send_transport);
  if (it == transports_.end()) {
    return false;
  }

  // Reconfigure signals to propagate send-side signals from |send_transport|,
  // and not from any previous |send_transport_|.
  if (send_transport_) {
    send_transport_->SignalWritableState.disconnect(this);
    send_transport_->SignalReadyToSend.disconnect(this);
    send_transport_->SignalSentPacket.disconnect(this);
  }

  send_transport_ = send_transport;
  send_transport_->SignalWritableState.connect(
      this, &CompositePacketTransport::OnWritableState);
  send_transport_->SignalReadyToSend.connect(
      this, &CompositePacketTransport::OnReadyToSend);
  send_transport_->SignalSentPacket.connect(
      this, &CompositePacketTransport::OnSentPacket);

  // We may need to indicate to the application that we're now ready to send.
  SignalWritableState(this);
  if (writable()) {
    SignalReadyToSend(this);
  }
  return true;
}

const std::string& CompositePacketTransport::transport_name() const {
  return transports_.front()->transport_name();
}

bool CompositePacketTransport::writable() const {
  return send_transport_ && send_transport_->writable();
}

bool CompositePacketTransport::receiving() const {
  bool recv = false;
  for (const PacketTransportInternal* transport : transports_) {
    recv |= transport->receiving();
  }
  return recv;
}

int CompositePacketTransport::SendPacket(const char* data,
                                         size_t len,
                                         const rtc::PacketOptions& options,
                                         int flags) {
  if (!send_transport_) {
    error_ = ENOTCONN;
    return -1;
  }
  return send_transport_->SendPacket(data, len, options, flags);
}

int CompositePacketTransport::SetOption(rtc::Socket::Option opt, int value) {
  for (auto transport : transports_) {
    transport->SetOption(opt, value);
  }
  return 0;
}

bool CompositePacketTransport::GetOption(rtc::Socket::Option opt, int* value) {
  // Return the first value found for the desired option.  SetOption should keep
  // options in sync between different transports, but if some transports drop
  // or ignore options, we want to reflect the state of the transports that
  // actually support those options.
  for (const auto transport : transports_) {
    if (transport->GetOption(opt, value)) {
      return true;
    }
  }
  return false;
}

int CompositePacketTransport::GetError() {
  // If we have our own error (eg. tried to send without setting
  // |send_transport_|) return and clear it.
  if (error_) {
    int val = error_;
    error_ = 0;
    return val;
  }

  // If any of our transports has an error, return the first one.
  for (auto transport : transports_) {
    // GetError is not const, and may clear the error, so we can only call it
    // once.
    int error = transport->GetError();
    if (error != 0) {
      return error;
    }
  }
  return 0;
}

absl::optional<NetworkRoute> CompositePacketTransport::network_route() const {
  // This should not be used for transports with different underlying ICE
  // transports, so the network routes should all be the same anyway.
  return transports_.front()->network_route();
}

void CompositePacketTransport::OnReceivingState(
    PacketTransportInternal* transport) {
  SignalReceivingState(this);
}

void CompositePacketTransport::OnReadPacket(PacketTransportInternal* transport,
                                            const char* packet,
                                            size_t size,
                                            const int64_t& packet_time,
                                            int flags) {
  SignalReadPacket(this, packet, size, packet_time, flags);
}

void CompositePacketTransport::OnNetworkRouteChanged(
    absl::optional<NetworkRoute> route) {
  SignalNetworkRouteChanged(route);
}

void CompositePacketTransport::OnWritableState(
    PacketTransportInternal* transport) {
  RTC_DCHECK_EQ(transport, send_transport_);
  SignalWritableState(this);
}

void CompositePacketTransport::OnReadyToSend(
    PacketTransportInternal* transport) {
  RTC_DCHECK_EQ(transport, send_transport_);
  SignalReadyToSend(this);
}

void CompositePacketTransport::OnSentPacket(PacketTransportInternal* transport,
                                            const rtc::SentPacket& packet) {
  RTC_DCHECK_EQ(transport, send_transport_);
  SignalSentPacket(this, packet);
}

}  // namespace rtc
