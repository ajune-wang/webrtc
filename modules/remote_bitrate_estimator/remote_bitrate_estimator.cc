/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"

#include "api/rtp_headers.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

BwePacket ToBwePacket(const RtpPacketReceived& rtp_packet) {
  BwePacket packet;
  packet.arrival_time_ms = rtp_packet.arrival_time_ms();
  packet.payload_size = rtp_packet.payload_size() + rtp_packet.padding_size();
  packet.total_size = rtp_packet.size();
  packet.ssrc = rtp_packet.Ssrc();
  packet.rtp_timestamp = rtp_packet.Timestamp();
  packet.transmission_time_offset =
      rtp_packet.GetExtension<TransmissionOffset>();
  packet.absolute_send_time = rtp_packet.GetExtension<AbsoluteSendTime>();

  packet.transport_sequence_number.emplace();
  if (!rtp_packet.GetExtension<TransportSequenceNumberV2>(
          &*packet.transport_sequence_number, &packet.feedback_request) &&
      !rtp_packet.GetExtension<TransportSequenceNumber>(
          &*packet.transport_sequence_number)) {
    packet.transport_sequence_number = absl::nullopt;
  }
  return packet;
}

BwePacket ToBwePacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header) {
  BwePacket packet;
  packet.arrival_time_ms = arrival_time_ms;
  packet.payload_size = payload_size;
  packet.total_size = payload_size + header.headerLength;
  packet.ssrc = header.ssrc;
  packet.rtp_timestamp = header.timestamp;
  if (header.extension.hasTransmissionTimeOffset)
    packet.transmission_time_offset = header.extension.transmissionTimeOffset;
  if (header.extension.hasAbsoluteSendTime)
    packet.absolute_send_time = header.extension.absoluteSendTime;
  if (header.extension.hasTransportSequenceNumber)
    packet.transport_sequence_number = header.extension.transportSequenceNumber;
  packet.feedback_request = header.feedback_request;
  return packet;
}

}  // namespace webrtc
