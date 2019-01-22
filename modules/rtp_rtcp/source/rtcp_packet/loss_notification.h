/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_LOSS_NOTIFICATION_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_LOSS_NOTIFICATION_H_

#include "modules/rtp_rtcp/source/rtcp_packet/psfb.h"
#include "rtc_base/system/unused.h"

namespace webrtc {
namespace rtcp {
class CommonHeader;

class LossNotification : public Psfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 15;
  static constexpr size_t kMaxNumberOfSsrcs = 0xff;  // TODO: !!!

  LossNotification();
  LossNotification(const LossNotification& other);
  ~LossNotification() override;

  size_t BlockLength() const override;

  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              PacketReadyCallback callback) const override
      RTC_WARN_UNUSED_RESULT;

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& packet) RTC_WARN_UNUSED_RESULT;

  // Set all of the values transmitted by the loss notification message.
  // If the values may not be represented by a loss notification message,
  // false is returned, and no change is made to the object; this happens
  // when |last_recieved| is ahead of |last_decoded| by more than 0x7fff.
  // This is because |last_recieved| is represented on the wire as a delta,
  // and only 15 bits are available for that delta.
  bool Set(uint16_t last_decoded,
           uint16_t last_received,
           bool decodability_flag) RTC_WARN_UNUSED_RESULT;

  uint16_t last_decoded() const { return last_decoded_; }
  uint16_t last_received() const { return last_received_; }
  bool decodability_flag() const { return decodability_flag_; }

 private:
  static constexpr uint32_t kUniqueIdentifier = 0x4C4E5446;  // 'L' 'N' 'T' 'F'.

  // Media ssrc is unused, shadow base class setter and getter to cause
  // a link time error if these are used.
  void SetMediaSsrc(uint32_t);
  uint32_t media_ssrc() const;

  uint16_t last_decoded_;
  uint16_t last_received_;
  bool decodability_flag_;
};
}  // namespace rtcp
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_LOSS_NOTIFICATION_H_
