/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_COMPOUND_PACKET_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_COMPOUND_PACKET_H_

#include <memory>
#include <vector>

#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "rtc_base/buffer.h"

namespace webrtc {
namespace rtcp {

class CompoundPacket {
 public:
  CompoundPacket() = default;
  CompoundPacket(const CompoundPacket&) = delete;
  CompoundPacket& operator=(const CompoundPacket&) = delete;
  ~CompoundPacket() = default;

  void Append(std::unique_ptr<RtcpPacket> packet);
  rtc::Buffer Build() const;

 private:
  std::vector<std::unique_ptr<RtcpPacket>> packets_;
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_COMPOUND_PACKET_H_
