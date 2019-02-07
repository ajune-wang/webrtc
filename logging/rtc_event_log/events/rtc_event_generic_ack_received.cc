/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_generic_ack_received.h"

namespace webrtc {

RtcEventGenericAckReceived::RtcEventGenericAckReceived(
    int64_t packet_number,
    const std::vector<AckedPacket>& received_acks)
    : packet_number_(packet_number), received_acks_(received_acks) {}

RtcEventGenericAckReceived::~RtcEventGenericAckReceived() = default;

RtcEvent::Type RtcEventGenericAckReceived::GetType() const {
  return RtcEvent::Type::GenericAckReceived;
}

bool RtcEventGenericAckReceived::IsConfigEvent() const {
  return false;
}

}  // namespace webrtc
