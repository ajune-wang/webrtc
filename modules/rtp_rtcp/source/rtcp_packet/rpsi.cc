/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/rpsi.h"

#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"

namespace webrtc {
namespace rtcp {
constexpr uint8_t Rpsi::kFeedbackMessageType;

// RFC 4585: Feedback format.
// Common packet format:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|   FMT   |       PT      |          length               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                  SSRC of packet sender                        |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |             SSRC of media source (unused) = 0                 |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  :            Feedback Control Information (FCI)                 :
//  :                                                               :
//
// Reference picture selection indication
// RFC 4585: section 6.3.3. Inidcated by PT=PSBF & FMT=3, and there
// must be exactly one RPSI contained in the FCI field.
//
// FCI:
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |      PB       |0| Payload Type|    Native RPSI bit string     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   defined per codec          ...                              |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// RFC 7798, section 8.3 defines the "Native RPSI bit string" for H.265,
// a base16 representation of 8-bits containing 2 MSB equal to 0, and 6
// bits of nuh_layer_id, followed by 32-bits representing the PicOrderCntVal
// in network byte order, for the picture that is requested to be used as
// reference frame during encoding. The usage of indicating successfully
// decoded picture is deprecated.
// There is no spec defining RPSI bit string for H.264 AFIAK, we temporarily
// use the same format as H.265, but replacing nuh_layer_id with TID as
// defined in RFC 6190, section 1.1.3.

Rpsi::Rpsi() = default;

Rpsi::Rpsi(const Rpsi& pli) = default;

Rpsi::~Rpsi() = default;

bool Rpsi::Parse(const CommonHeader& packet) {
  RTC_DCHECK_EQ(packet.type(), kPacketType);
  RTC_DCHECK_EQ(packet.fmt(), kFeedbackMessageType);

  if (packet.payload_size_bytes() < kCommonFeedbackLength + 8) {
    RTC_LOG(LS_ERROR) << "Packet is too small to be a valid RPSI message.";
    return false;
  }

  // Common sender and media SSRC part of Psfb.
  ParseCommonFeedback(packet.payload());
  uint8_t padding_bits = packet.payload()[kPaddingSizeOffset];
  if (padding_bits != kPaddingInBits) {
    RTC_LOG(LS_ERROR) << "Padding must be 8-bits for HEVC RPSI.";
    return false;
  }

  size_t padding_bytes = padding_bits / 8;
  if (padding_bytes + kBitStringOffset >= packet.payload_size_bytes()) {
    RTC_LOG(LS_ERROR) << "RPSI payload size invalid.";
    return false;
  }

  payload_type_ = packet.payload()[kPayloadTypeOffset] & 0x7f;

  // First byte of "Native RPSI bit string defined per codec" is base16
  // encoded.
  uint8_t layer_id_high8 = 0, layer_id_low8 = 0;
  bool decode_result = rtc::hex_decode(
      static_cast<char>(packet.payload()[kBitStringOffset]), layer_id_high8);
  if (!decode_result) {
    RTC_LOG(LS_ERROR) << "Invalid layer id.";
    return false;
  }
  decode_result = rtc::hex_decode(
      static_cast<char>(packet.payload()[kBitStringOffset + 1]), layer_id_low8);
  if (!decode_result) {
    RTC_LOG(LS_ERROR) << "Invalid layer id.";
    return false;
  }

  // Low 6-bits is nuh_layer_id for H.265 or TID for H.264.
  layer_id_ = ((layer_id_high8 & 0xf) << 4) | (layer_id_low8 & 0xf);

  // 32-bits PicOrderCntVal of requested reference frame.
  uint32_t pic_order_cnt = ByteReader<uint32_t>::ReadBigEndian(
      packet.payload() + kBitStringOffset + 2);
  picture_order_cnt_ = pic_order_cnt;

  return true;
}

bool Rpsi::Create(uint8_t* packet,
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

  // PB
  ByteWriter<uint8_t>::WriteBigEndian(packet + *index, 0);
  *index += sizeof(uint8_t);

  // Payload Type
  ByteWriter<uint8_t>::WriteBigEndian(packet + *index, payload_type_);
  *index += sizeof(uint8_t);

  // nuh_layer_id
  uint8_t layer_id = layer_id_ & 0b0011'1111;
  ByteWriter<unsigned char>::WriteBigEndian(
      packet + *index,
      static_cast<unsigned char>(rtc::hex_encode(layer_id >> 4)));
  *index += sizeof(unsigned char);
  ByteWriter<unsigned char>::WriteBigEndian(
      packet + *index,
      static_cast<unsigned char>(rtc::hex_encode(layer_id & 0xf)));
  *index += sizeof(unsigned char);

  // PicOrderCntVal, no padding.
  ByteWriter<uint32_t>::WriteBigEndian(packet + *index, picture_order_cnt_);
  *index += sizeof(uint32_t);

  return true;
}

void Rpsi::SetPayloadType(uint8_t payload) {
  RTC_DCHECK_LE(payload, 0x7f);
  payload_type_ = payload;
}

void Rpsi::SetLayerId(uint8_t layer) {
  layer_id_ = layer;
}

void Rpsi::SetPictureOrderCnt(uint32_t pic_order_cnt) {
  picture_order_cnt_ = pic_order_cnt;
}

size_t Rpsi::BlockLength() const {
  return kHeaderLength + kCommonFeedbackLength + kFciInBytes;
}

}  // namespace rtcp
}  // namespace webrtc
