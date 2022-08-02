/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/compound_packet.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace rtcp {

void CompoundPacket::Append(std::unique_ptr<RtcpPacket> packet) {
  RTC_CHECK(packet);
  packets_.push_back(std::move(packet));
}

rtc::Buffer CompoundPacket::Build() const {
  size_t size = 0;
  for (const std::unique_ptr<RtcpPacket>& rtcp_packet : packets_) {
    size += rtcp_packet->BlockLength();
  }
  rtc::Buffer buffer(size);
  size_t index = 0;
  uint8_t* data = buffer.data();
  for (const std::unique_ptr<RtcpPacket>& rtcp_packet : packets_) {
    RTC_CHECK(rtcp_packet->Create(
        data, &index, size, [](rtc::ArrayView<const uint8_t>) {
          RTC_CHECK(false) << "Unexpected fragmentation";
        }));
  }
  RTC_CHECK_EQ(index, size);
  return buffer;
}

}  // namespace rtcp
}  // namespace webrtc
