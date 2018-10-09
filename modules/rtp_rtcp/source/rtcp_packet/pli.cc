/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/pli.h"

#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace rtcp {
constexpr uint8_t Pli::kFeedbackMessageType;
// RFC 4585: Feedback format.
//
// Common packet format:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|   FMT   |       PT      |          length               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                  SSRC of packet sender                        |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                  SSRC of media source                         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  :            Feedback Control Information (FCI)                 :
//  :                                                               :

//
// Picture loss indication (PLI) (RFC 4585).
// FCI: no feedback control information.

Pli::Pli()
    : ltr_recovery_experiment_(
          webrtc::field_trial::IsEnabled("WebRTC-LtrRecoveryExperiment")) {}

Pli::~Pli() = default;

bool Pli::Parse(const CommonHeader& packet) {
  RTC_DCHECK_EQ(packet.type(), kPacketType);
  RTC_DCHECK_EQ(packet.fmt(), kFeedbackMessageType);

  if (packet.payload_size_bytes() < kCommonFeedbackLength) {
    RTC_LOG(LS_WARNING) << "Packet is too small to be a valid PLI packet";
    return false;
  }

  ParseCommonFeedback(packet.payload());

  if (ltr_recovery_experiment_) {
    last_decoded_packet_sequence_number_.reset();
    last_received_packet_sequence_number_.reset();

    if (packet.payload_size_bytes() >= kCommonFeedbackLength + 4) {
      const uint8_t* payload = packet.payload() + kCommonFeedbackLength;
      last_decoded_packet_sequence_number_ =
          ByteReader<uint16_t>::ReadBigEndian(payload);
      payload += 2;
      last_received_packet_sequence_number_ =
          ByteReader<uint16_t>::ReadBigEndian(payload);
    }
  }

  return true;
}

size_t Pli::BlockLength() const {
  size_t block_length = kHeaderLength + kCommonFeedbackLength;
  if (ltr_recovery_experiment_ && last_decoded_packet_sequence_number_ &&
      last_received_packet_sequence_number_) {
    block_length += 4;
  }
  return block_length;
}

void Pli::SetLastDecodedPacketSequenceNumber(uint16_t sequence_number) {
  RTC_CHECK(ltr_recovery_experiment_);
  last_decoded_packet_sequence_number_ = sequence_number;
}

void Pli::SetLastReceivedPacketSequenceNumber(uint16_t sequence_number) {
  RTC_CHECK(ltr_recovery_experiment_);
  last_received_packet_sequence_number_ = sequence_number;
}

bool Pli::Create(uint8_t* packet,
                 size_t* index,
                 size_t max_length,
                 PacketReadyCallback callback) const {
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }

  CreateHeader(kFeedbackMessageType, kPacketType, HeaderLength(), packet,
               index);
  CreateCommonFeedback(packet + *index);
  *index += kCommonFeedbackLength;

  if (ltr_recovery_experiment_ && last_decoded_packet_sequence_number_ &&
      last_received_packet_sequence_number_) {
    ByteWriter<uint16_t>::WriteBigEndian(packet + *index,
                                         *last_decoded_packet_sequence_number_);
    *index += 2;
    ByteWriter<uint16_t>::WriteBigEndian(
        packet + *index, *last_received_packet_sequence_number_);
    *index += 2;
  }

  return true;
}

}  // namespace rtcp
}  // namespace webrtc
