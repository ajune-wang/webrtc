/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_generic_packet_sent.h"

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"

namespace webrtc {

RtcEventGenericPacketSent::RtcEventGenericPacketSent(int64_t packet_number,
                                                     size_t packet_length,
                                                     size_t payload_length,
                                                     size_t padding_length,
                                                     bool has_ack)
    : packet_number_(packet_number),
      packet_length_(packet_length),
      payload_length_(payload_length),
      padding_length_(padding_length),
      has_ack_(has_ack) {}

RtcEventGenericPacketSent::~RtcEventGenericPacketSent() = default;

RtcEvent::Type RtcEventGenericPacketSent::GetType() const {
  return RtcEvent::Type::GenericPacketSent;
}

bool RtcEventGenericPacketSent::IsConfigEvent() const {
  return false;
}

}  // namespace webrtc
