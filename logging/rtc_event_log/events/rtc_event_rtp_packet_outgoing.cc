/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"

namespace webrtc {

RtcEventRtpPacketOutgoing::RtcEventRtpPacketOutgoing(const RtpPacket& packet,
                                                     int probe_cluster_id)
    : header_(packet), probe_cluster_id_(probe_cluster_id) {
  RTC_DCHECK_EQ(packet.size(),
                payload_length() + header_length() + padding_length());
}

RtcEventRtpPacketOutgoing::RtcEventRtpPacketOutgoing(
    const RtcEventRtpPacketOutgoing& other) = default;

RtcEventRtpPacketOutgoing::~RtcEventRtpPacketOutgoing() = default;

RtcEvent::Type RtcEventRtpPacketOutgoing::GetType() const {
  return RtcEvent::Type::RtpPacketOutgoing;
}

bool RtcEventRtpPacketOutgoing::IsConfigEvent() const {
  return false;
}

std::unique_ptr<RtcEventRtpPacketOutgoing> RtcEventRtpPacketOutgoing::Copy()
    const {
  return absl::WrapUnique<RtcEventRtpPacketOutgoing>(
      new RtcEventRtpPacketOutgoing(*this));
}

}  // namespace webrtc
