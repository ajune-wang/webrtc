/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_RTP_RTP_PACKET_TYPE_H_
#define API_TRANSPORT_RTP_RTP_PACKET_TYPE_H_

#include <stdint.h>

#include "absl/strings/string_view.h"
#include "api/array_view.h"

namespace webrtc {

enum class RtpPacketType {
  kRtp,
  kRtcp,
  kUnknown,
};

// Checks the packet header to determine if it can be an RTP or RTCP packet.
RtpPacketType InferRtpPacketType(rtc::ArrayView<const uint8_t> packet);

// Returns "RTCP", "RTP" or "Unknown" according to |packet_type|.
absl::string_view RtpPacketTypeToString(RtpPacketType packet_type);

inline bool IsRtpPacket(rtc::ArrayView<const uint8_t> packet) {
  return InferRtpPacketType(packet) == RtpPacketType::kRtp;
}

inline bool IsRtcpPacket(rtc::ArrayView<const uint8_t> packet) {
  return InferRtpPacketType(packet) == RtpPacketType::kRtcp;
}

}  // namespace webrtc

#endif  // API_TRANSPORT_RTP_RTP_PACKET_TYPE_H_
