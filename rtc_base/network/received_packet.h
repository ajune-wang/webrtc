/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_NETWORK_RECEIVED_PACKET_H_
#define RTC_BASE_NETWORK_RECEIVED_PACKET_H_

#include <cstdint>

#include "api/array_view.h"
#include "api/units/timestamp.h"

namespace rtc {

class ReceivedPacket {
 public:
  ReceivedPacket(rtc::ArrayView<const uint8_t> payload,
                 webrtc::Timestamp arrival_time);

  rtc::ArrayView<const uint8_t> payload() const { return payload_; }
  webrtc::Timestamp arrival_time() const { return arrival_time_; }

 private:
  rtc::ArrayView<const uint8_t> payload_;
  webrtc::Timestamp arrival_time_;
};

}  // namespace rtc
#endif  // RTC_BASE_NETWORK_RECEIVED_PACKET_H_
