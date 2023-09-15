/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_RPSI_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_RPSI_H_

#include "modules/rtp_rtcp/source/rtcp_packet/psfb.h"

namespace webrtc {
namespace rtcp {

class CommonHeader;

// Reference Picuture Selection Indication(RPSI) (RFC 4585).
class Rpsi : public Psfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 3;

  Rpsi();
  Rpsi(const Rpsi& pli);
  ~Rpsi() override;

  bool Parse(const CommonHeader& packet);

  void SetPayloadType(uint8_t payload);
  void SetLayerId(uint8_t layer);
  void SetPictureOrderCnt(uint32_t pic_order_cnt);

  uint8_t payload_type() const { return payload_type_; }
  uint8_t layer_id() const { return layer_id_; }
  uint64_t picture_order_cnt() const { return picture_order_cnt_; }

  size_t BlockLength() const override;

  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              PacketReadyCallback callback) const override;

 private:
  static constexpr size_t kPaddingSizeOffset = 8;
  static constexpr size_t kPaddingInBits = 0;
  static constexpr size_t kPayloadTypeOffset = 9;
  static constexpr size_t kBitStringOffset = 10;
  static constexpr size_t kBitStringSizeInBytes = 6;
  // RFC 4585, RFC 7741 and RFC 8082 does not explicitly define
  // the length of RPSI FCI payload. We follow RFC 7798 which
  // specifies this to be 8-bytes.
  static constexpr size_t kFciInBytes = 8;

  uint8_t payload_type_ = 0;
  uint8_t layer_id_ = 0;
  uint32_t picture_order_cnt_ = 0;
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_RPSI_H_
