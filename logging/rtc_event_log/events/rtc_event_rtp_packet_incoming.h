/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_INCOMING_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_INCOMING_H_

#include <stddef.h>                                       // for size_t
#include <memory>                                         // for unique_ptr

#include "logging/rtc_event_log/events/rtc_event.h"       // for RtcEvent
#include "modules/rtp_rtcp/source/rtp_packet.h"           // for RtpPacket
#include "modules/rtp_rtcp/source/rtp_packet_received.h"  // for RtpPacketRe...

namespace webrtc {

class RtcEventRtpPacketIncoming final : public RtcEvent {
 public:
  explicit RtcEventRtpPacketIncoming(const RtpPacketReceived& packet);
  ~RtcEventRtpPacketIncoming() override;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  std::unique_ptr<RtcEvent> Copy() const override;

  RtpPacket header_;            // Only the packet's header will be stored here.
  const size_t packet_length_;  // Length before stripping away all but header.

 private:
  RtcEventRtpPacketIncoming(const RtcEventRtpPacketIncoming& other);
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_INCOMING_H_
