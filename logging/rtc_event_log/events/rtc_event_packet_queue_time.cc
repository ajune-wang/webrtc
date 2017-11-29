/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_packet_queue_time.h"

namespace webrtc {

RtcEventPacketQueueTime::RtcEventPacketQueueTime(uint32_t ssrc,
                                                 int64_t queue_time_ms)
    : ssrc_(ssrc), queue_time_ms_(queue_time_ms) {}

RtcEventPacketQueueTime::~RtcEventPacketQueueTime() = default;

RtcEvent::Type RtcEventPacketQueueTime::GetType() const {
  return RtcEvent::Type::PacketQueueTime;
}

bool RtcEventPacketQueueTime::IsConfigEvent() const {
  return false;
}

}  // namespace webrtc
