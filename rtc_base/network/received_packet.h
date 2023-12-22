/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_NETWORK_RECEIVED_PACKET_H_
#define RTC_BASE_NETWORK_RECEIVED_PACKET_H_

#include <cstdint>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/units/timestamp.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {

// ReceivedPacket repressent a received IP packet.
// It contains a payload and metadata.
// ReceivedPacket itself does not put constraints on what payload contains. For
// example it may contains STUN, SCTP, SRTP, RTP, RTCP.... etc.
class RTC_EXPORT ReceivedPacket {
 public:
  // Caller must keep memory pointed to by payload and address valid for the
  // lifetime of this ReceivedPacket.
  ReceivedPacket(rtc::ArrayView<const uint8_t> payload,
                 const SocketAddress& source_address,
                 absl::optional<webrtc::Timestamp> arrival_time = absl::nullopt,
                 absl::optional<bool> ect_ce = absl::nullopt);

  // Address/port of the packet sender.
  const SocketAddress& source_address() const { return source_address_; }
  rtc::ArrayView<const uint8_t> payload() const { return payload_; }

  // L4S ECT marking. https://www.rfc-editor.org/rfc/rfc9331.html
  // If not set, L4S is not used or not supported by the network.
  // if set and false: ECN bits are ECT(1)
  // If set and true: ECN bits are CE
  absl::optional<bool> ect_ce() const { return ect_ce_; }

  // Timestamp when this packet was received. Not available on all socket
  // implementations.
  absl::optional<webrtc::Timestamp> arrival_time() const {
    return arrival_time_;
  }

  static ReceivedPacket CreateFromLegacy(
      const char* data,
      size_t size,
      int64_t packet_time_us,
      const rtc::SocketAddress& addr = rtc::SocketAddress()) {
    return CreateFromLegacy(reinterpret_cast<const uint8_t*>(data), size,
                            packet_time_us, addr);
  }

  static ReceivedPacket CreateFromLegacy(
      const uint8_t* data,
      size_t size,
      int64_t packet_time_us,
      const rtc::SocketAddress& = rtc::SocketAddress());

 private:
  rtc::ArrayView<const uint8_t> payload_;
  absl::optional<webrtc::Timestamp> arrival_time_;
  const SocketAddress& source_address_;
  absl::optional<bool> ect_ce_;
};

}  // namespace rtc
#endif  // RTC_BASE_NETWORK_RECEIVED_PACKET_H_
