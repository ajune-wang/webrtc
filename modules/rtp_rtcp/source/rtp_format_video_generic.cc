/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/logging.h"

namespace webrtc {

static const size_t kGenericHeaderLength = 1;
static const size_t kExtendedHeaderLength = 2;

RtpPacketizerGeneric::RtpPacketizerGeneric(
    rtc::ArrayView<const uint8_t> payload,
    PayloadSizeLimits limits,
    const RTPVideoHeader& rtp_video_header,
    FrameType frame_type)
    : picture_id_(rtp_video_header.generic
                      ? absl::optional<uint16_t>(
                            rtp_video_header.generic->frame_id & 0x7FFF)
                      : absl::nullopt),
      remaining_payload_(payload) {
  generic_header_ = RtpFormatVideoGeneric::kFirstPacketBit;
  if (frame_type == kVideoFrameKey) {
    generic_header_ |= RtpFormatVideoGeneric::kKeyFrameBit;
  }
  if (picture_id_.has_value()) {
    generic_header_ |= RtpFormatVideoGeneric::kExtendedHeaderBit;
  }

  limits.max_payload_len -= kGenericHeaderLength;
  if (picture_id_.has_value())
    limits.max_payload_len -= kExtendedHeaderLength;
  payload_sizes_ = SplitAboutEqually(payload.size(), limits);
  current_packet_ = payload_sizes_.begin();
}

RtpPacketizerGeneric::~RtpPacketizerGeneric() = default;

size_t RtpPacketizerGeneric::NumPackets() const {
  return payload_sizes_.end() - current_packet_;
}

bool RtpPacketizerGeneric::NextPacket(RtpPacketToSend* packet) {
  RTC_DCHECK(packet);
  if (current_packet_ == payload_sizes_.end())
    return false;

  size_t next_packet_payload_len = *current_packet_;

  size_t total_length = next_packet_payload_len + kGenericHeaderLength +
                        (picture_id_.has_value() ? kExtendedHeaderLength : 0);
  uint8_t* out_ptr = packet->AllocatePayload(total_length);

  // Put generic header in packet.
  out_ptr[0] = generic_header_;
  out_ptr += kGenericHeaderLength;

  if (picture_id_.has_value()) {
    WriteExtendedHeader(out_ptr);
    out_ptr += kExtendedHeaderLength;
  }

  // Remove first-packet bit, following packets are intermediate.
  generic_header_ &= ~RtpFormatVideoGeneric::kFirstPacketBit;

  // Put payload in packet.
  memcpy(out_ptr, remaining_payload_.data(), next_packet_payload_len);
  remaining_payload_ = remaining_payload_.subview(next_packet_payload_len);

  ++current_packet_;

  // Packets left to produce and data left to split should end at the same time.
  RTC_DCHECK_EQ(current_packet_ == payload_sizes_.end(),
                remaining_payload_.empty());

  packet->SetMarker(remaining_payload_.empty());
  return true;
}

void RtpPacketizerGeneric::WriteExtendedHeader(uint8_t* out_ptr) {
  // Store bottom 15 bits of the the sequence number. Only 15 bits are used for
  // compatibility with other packetizer implemenetations that also use 15 bits.
  out_ptr[0] = (*picture_id_ >> 8) & 0x7F;
  out_ptr[1] = *picture_id_ & 0xFF;
}

RtpDepacketizerGeneric::~RtpDepacketizerGeneric() = default;

bool RtpDepacketizerGeneric::Parse(ParsedPayload* parsed_payload,
                                   const uint8_t* payload_data,
                                   size_t payload_data_length) {
  assert(parsed_payload != NULL);
  if (payload_data_length == 0) {
    RTC_LOG(LS_WARNING) << "Empty payload.";
    return false;
  }

  uint8_t generic_header = *payload_data++;
  --payload_data_length;

  parsed_payload->frame_type =
      ((generic_header & RtpFormatVideoGeneric::kKeyFrameBit) != 0)
          ? kVideoFrameKey
          : kVideoFrameDelta;
  parsed_payload->video_header().is_first_packet_in_frame =
      (generic_header & RtpFormatVideoGeneric::kFirstPacketBit) != 0;
  parsed_payload->video_header().codec = kVideoCodecGeneric;
  parsed_payload->video_header().width = 0;
  parsed_payload->video_header().height = 0;

  if (generic_header & RtpFormatVideoGeneric::kExtendedHeaderBit) {
    if (payload_data_length < kExtendedHeaderLength) {
      RTC_LOG(LS_WARNING) << "Too short payload for generic header.";
      return false;
    }
    parsed_payload->video_header().generic.emplace();
    parsed_payload->video_header().generic->frame_id =
        ((payload_data[0] & 0x7F) << 8) | payload_data[1];
    payload_data += kExtendedHeaderLength;
    payload_data_length -= kExtendedHeaderLength;
  }

  parsed_payload->payload = payload_data;
  parsed_payload->payload_length = payload_data_length;
  return true;
}
}  // namespace webrtc
