/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/packet_transport_internal.h"

#include "api/sequence_checker.h"

namespace rtc {

PacketTransportInternal::PacketTransportInternal() = default;

PacketTransportInternal::~PacketTransportInternal() = default;

bool PacketTransportInternal::GetOption(rtc::Socket::Option opt, int* value) {
  return false;
}

absl::optional<NetworkRoute> PacketTransportInternal::network_route() const {
  return absl::optional<NetworkRoute>();
}

void PacketTransportInternal::NotifyPacketReceived(
    const rtc::ReceivedPacket& packet,
    const TransportInfo& transport_info) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  if (received_packet_callbacklist_count_ == 0) {
    // TODO(bugs.webrtc.org:15368): Replace with
    // received_packet_callbacklist_.
    SignalReadPacket(this,
                     reinterpret_cast<const char*>(packet.payload().data()),
                     packet.payload().size(),
                     packet.arrival_time() ? packet.arrival_time()->us() : -1,
                     transport_info.flags);
  } else {
    RTC_DCHECK(SignalReadPacket.is_empty());
    received_packet_callbacklist_.Send(this, packet, transport_info);
  }
}

}  // namespace rtc
