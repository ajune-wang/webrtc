/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_GENERIC_PACKET_SENT_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_GENERIC_PACKET_SENT_H_

#include "logging/rtc_event_log/events/rtc_event.h"

namespace webrtc {

class RtcEventGenericPacketSent final : public RtcEvent {
 public:
  RtcEventGenericPacketSent(int64_t packet_number,
                            size_t packet_length,
                            size_t payload_length,
                            size_t padding_length,
                            bool has_ack);
  RtcEventGenericPacketSent(const RtcEventGenericPacketSent& packet);
  ~RtcEventGenericPacketSent() override;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  // An identifier of the packet.
  int64_t packet_number() const { return packet_number_; }

  // Total packet length, including all packetization overheads, but not
  // including ICE/TURN/IP overheads.
  size_t packet_length() const { return packet_length_; }

  // Total length of payload sent (size of raw data), without packetization
  // overheads. In other words, sum of video/audio/data frame lengths in the
  // packet. This may still include serialization overheads.
  size_t payload_length() const { return payload_length_; }

  size_t padding_length() const { return padding_length_; }

  bool has_ack() const { return has_ack_; }

 private:
  const int64_t packet_number_;
  const size_t packet_length_;
  const size_t payload_length_;
  const size_t padding_length_;
  bool has_ack_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_GENERIC_PACKET_SENT_H_
