/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_PLI_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_PLI_H_

#include "absl/types/optional.h"
#include "modules/rtp_rtcp/source/rtcp_packet/psfb.h"

namespace webrtc {
namespace rtcp {
class CommonHeader;
// Picture loss indication (PLI) (RFC 4585).
class Pli : public Psfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 1;

  explicit Pli(bool ltr_recovery_experiment = false);
  ~Pli() override;

  bool Parse(const CommonHeader& packet);

  size_t BlockLength() const override;

  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              PacketReadyCallback callback) const override;

  absl::optional<uint16_t> LastDecodedPacketSequenceNumber() const {
    return last_decoded_packet_sequence_number_;
  }

  absl::optional<uint16_t> LastReceivedPacketSequenceNumber() const {
    return last_received_packet_sequence_number_;
  }

  void SetLastDecodedPacketSequenceNumber(uint16_t sequence_number);
  void SetLastReceivedPacketSequenceNumber(uint16_t sequence_number);

 private:
  const bool ltr_recovery_experiment_;

  absl::optional<uint16_t> last_decoded_packet_sequence_number_;
  absl::optional<uint16_t> last_received_packet_sequence_number_;
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_PLI_H_
