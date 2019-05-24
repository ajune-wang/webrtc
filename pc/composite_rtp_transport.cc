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

#include "pc/composite_rtp_transport.h"

#include "absl/memory/memory.h"
#include "p2p/base/composite_packet_transport.h"
#include "p2p/base/packet_transport_internal.h"

namespace webrtc {

CompositeRtpTransport::CompositeRtpTransport(
    std::vector<RtpTransportInternal*> transports)
    : transports_(std::move(transports)) {
  std::vector<rtc::PacketTransportInternal*> rtp_transports;
  std::vector<rtc::PacketTransportInternal*> rtcp_transports;
  for (RtpTransportInternal* transport : transports_) {
    RTC_DCHECK(transport->rtcp_mux_enabled() == rtcp_mux_enabled())
        << "Either all or none of the transports in a composite must enable "
           "rtcp mux";

    transport->SignalNetworkRouteChanged.connect(
        this, &CompositeRtpTransport::OnNetworkRouteChanged);
    transport->SignalRtcpPacketReceived.connect(
        this, &CompositeRtpTransport::OnRtcpPacketReceived);

    rtp_transports.push_back(transport->rtp_packet_transport());
    if (transport->rtcp_packet_transport()) {
      rtcp_transports.push_back(transport->rtcp_packet_transport());
    }
  }

  rtp_packet_transport_ = absl::make_unique<rtc::CompositePacketTransport>(
      std::move(rtp_transports));
  if (!rtcp_mux_enabled() && !rtcp_transports.empty()) {
    rtcp_packet_transport_ = absl::make_unique<rtc::CompositePacketTransport>(
        std::move(rtcp_transports));
  }
}

bool CompositeRtpTransport::SetSendTransport(
    RtpTransportInternal* send_transport) {
  if (send_transport_ == send_transport) {
    return true;
  }

  auto it = std::find(transports_.begin(), transports_.end(), send_transport);
  if (it == transports_.end()) {
    return false;
  }

  if (send_transport_) {
    send_transport_->SignalReadyToSend.disconnect(this);
    send_transport_->SignalWritableState.disconnect(this);
    send_transport_->SignalSentPacket.disconnect(this);
  }

  send_transport_ = send_transport;
  send_transport_->SignalReadyToSend.connect(
      this, &CompositeRtpTransport::OnReadyToSend);
  send_transport_->SignalWritableState.connect(
      this, &CompositeRtpTransport::OnWritableState);
  send_transport_->SignalSentPacket.connect(
      this, &CompositeRtpTransport::OnSentPacket);

  bool result = rtp_packet_transport_->SetSendTransport(
      send_transport_->rtp_packet_transport());
  if (rtcp_packet_transport_) {
    result &= rtcp_packet_transport_->SetSendTransport(
        send_transport_->rtcp_packet_transport());
  }

  SignalWritableState(send_transport_->IsWritable(true) &&
                      send_transport_->IsWritable(false));
  if (send_transport_->IsReadyToSend()) {
    SignalReadyToSend(true);
  }

  return result;
}

bool CompositeRtpTransport::rtcp_mux_enabled() const {
  return transports_.front()->rtcp_mux_enabled();
}

void CompositeRtpTransport::SetRtcpMuxEnabled(bool enabled) {
  std::vector<rtc::PacketTransportInternal*> rtcp_transports;
  for (auto transport : transports_) {
    transport->SetRtcpMuxEnabled(enabled);
    if (transport->rtcp_packet_transport()) {
      rtcp_transports.push_back(transport->rtcp_packet_transport());
    }
  }
  if (enabled || rtcp_transports.empty()) {
    rtcp_packet_transport_ = nullptr;
  } else {
    RTC_DCHECK_EQ(transports_.size(), rtcp_transports.size());
    rtcp_packet_transport_ = absl::make_unique<rtc::CompositePacketTransport>(
        std::move(rtcp_transports));
  }
}

rtc::PacketTransportInternal* CompositeRtpTransport::rtp_packet_transport()
    const {
  return rtp_packet_transport_.get();
}

void CompositeRtpTransport::SetRtpPacketTransport(
    rtc::PacketTransportInternal* rtp) {
  RTC_NOTREACHED() << "SetRtpPacketTransport may not be used on a composite";
}

rtc::PacketTransportInternal* CompositeRtpTransport::rtcp_packet_transport()
    const {
  return rtcp_packet_transport_.get();
}

void CompositeRtpTransport::SetRtcpPacketTransport(
    rtc::PacketTransportInternal* rtcp) {
  RTC_NOTREACHED() << "SetRtcpPacketTransport may not be used on a composite";
}

bool CompositeRtpTransport::IsReadyToSend() const {
  return send_transport_ && send_transport_->IsReadyToSend();
}

bool CompositeRtpTransport::IsWritable(bool rtcp) const {
  return send_transport_ && send_transport_->IsWritable(rtcp);
}

bool CompositeRtpTransport::SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                                          const rtc::PacketOptions& options,
                                          int flags) {
  if (!send_transport_) {
    return false;
  }
  return send_transport_->SendRtpPacket(packet, options, flags);
}

bool CompositeRtpTransport::SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                                           const rtc::PacketOptions& options,
                                           int flags) {
  if (!send_transport_) {
    return false;
  }
  return send_transport_->SendRtcpPacket(packet, options, flags);
}

void CompositeRtpTransport::UpdateRtpHeaderExtensionMap(
    const cricket::RtpHeaderExtensions& header_extensions) {
  for (RtpTransportInternal* transport : transports_) {
    transport->UpdateRtpHeaderExtensionMap(header_extensions);
  }
}

bool CompositeRtpTransport::IsSrtpActive() const {
  bool active = true;
  for (RtpTransportInternal* transport : transports_) {
    active &= transport->IsSrtpActive();
  }
  return active;
}

bool CompositeRtpTransport::RegisterRtpDemuxerSink(
    const RtpDemuxerCriteria& criteria,
    RtpPacketSinkInterface* sink) {
  for (RtpTransportInternal* transport : transports_) {
    transport->RegisterRtpDemuxerSink(criteria, sink);
  }
  return true;
}

bool CompositeRtpTransport::UnregisterRtpDemuxerSink(
    RtpPacketSinkInterface* sink) {
  for (RtpTransportInternal* transport : transports_) {
    transport->UnregisterRtpDemuxerSink(sink);
  }
  return true;
}

void CompositeRtpTransport::OnNetworkRouteChanged(
    absl::optional<rtc::NetworkRoute> route) {
  SignalNetworkRouteChanged(route);
}

void CompositeRtpTransport::OnRtcpPacketReceived(rtc::CopyOnWriteBuffer* packet,
                                                 int64_t packet_time_us) {
  SignalRtcpPacketReceived(packet, packet_time_us);
}

void CompositeRtpTransport::OnWritableState(bool writable) {
  SignalWritableState(writable);
}

void CompositeRtpTransport::OnReadyToSend(bool ready_to_send) {
  SignalReadyToSend(ready_to_send);
}

void CompositeRtpTransport::OnSentPacket(const rtc::SentPacket& packet) {
  SignalSentPacket(packet);
}

}  // namespace webrtc
