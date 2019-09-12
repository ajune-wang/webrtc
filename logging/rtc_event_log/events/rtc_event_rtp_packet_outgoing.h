/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_OUTGOING_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_OUTGOING_H_

#include <memory>

#include "api/rtc_event_log/rtc_event.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"

namespace webrtc {

class RtcEventRtpPacketOutgoing final : public RtcEvent {
 public:
  RtcEventRtpPacketOutgoing(const RtpPacket& packet, int probe_cluster_id);
  ~RtcEventRtpPacketOutgoing() override;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  std::unique_ptr<RtcEventRtpPacketOutgoing> Copy() const;

  size_t packet_length() const {
    return payload_length() + header_length() + padding_length();
  }

  const RtpPacket& header() const { return header_; }
  size_t payload_length() const { return header_.payload_size(); }
  size_t header_length() const { return header_.headers_size(); }
  size_t padding_length() const { return header_.padding_size(); }
  int probe_cluster_id() const { return probe_cluster_id_; }

 private:
  RtcEventRtpPacketOutgoing(const RtcEventRtpPacketOutgoing& other);

  const RtpPacket header_;  // Only the packet's header will be used.
  // TODO(eladalon): Delete |probe_cluster_id_| along with legacy encoding.
  const int probe_cluster_id_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_OUTGOING_H_
