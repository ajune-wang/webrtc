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

#include <memory>

#include "api/rtc_event_log/rtc_event.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"

namespace webrtc {

class RtcEventRtpPacketIncoming final : public RtcEvent {
 public:
  explicit RtcEventRtpPacketIncoming(const RtpPacket& packet);
  ~RtcEventRtpPacketIncoming() override;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  std::unique_ptr<RtcEventRtpPacketIncoming> Copy() const;

  size_t packet_length() const { return header_.size(); }

  const RtpPacket& header() const { return header_; }
  size_t payload_length() const { return header_.payload_size(); }
  size_t header_length() const { return header_.headers_size(); }
  size_t padding_length() const { return header_.padding_size(); }

 private:
  RtcEventRtpPacketIncoming(const RtcEventRtpPacketIncoming& other);

  const RtpPacket header_;  // Only the packet's header will be used.
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_INCOMING_H_
