/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/rtp/rtp_packet_type.h"

namespace webrtc {

RtpPacketType InferRtpPacketType(rtc::ArrayView<const uint8_t> packet) {
  constexpr uint8_t kRtpVersion = 2;
  if (packet.size() < 2) {
    return RtpPacketType::kUnknown;
  }
  if ((packet[0] >> 6) != kRtpVersion) {
    return RtpPacketType::kUnknown;
  }

  // Check the RTP payload type. If 64 <= payload type < 96, it's RTCP.
  // For additional details, see http://tools.ietf.org/html/rfc5761.
  constexpr uint8_t kRtcpDifferentiatorMask = 0b0110'0000;
  constexpr uint8_t kRtcpPacketTypeIndicator = 0b0100'0000;
  if ((packet[1] & kRtcpDifferentiatorMask) == kRtcpPacketTypeIndicator) {
    constexpr size_t kMinRtcpPacketLen = 4;
    if (packet.size() < kMinRtcpPacketLen) {
      return RtpPacketType::kUnknown;
    }
    return RtpPacketType::kRtcp;
  } else {
    constexpr size_t kMinRtpPacketLen = 12;
    if (packet.size() < kMinRtpPacketLen) {
      return RtpPacketType::kUnknown;
    }
    return RtpPacketType::kRtp;
  }
}

absl::string_view RtpPacketTypeToString(RtpPacketType packet_type) {
  switch (packet_type) {
    case RtpPacketType::kRtp:
      return "RTP";
    case RtpPacketType::kRtcp:
      return "RTCP";
    case RtpPacketType::kUnknown:
      return "Unknown";
  }
}

}  // namespace webrtc
