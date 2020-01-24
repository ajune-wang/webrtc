/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packet_type.h"

#include <stdint.h>

#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr uint8_t kPcmuFrame[] = {
    0x80, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

constexpr uint8_t kInvalidPacket[] = {0x80, 0x00};

// A typical Receiver Report RTCP packet.
// PT=RR, LN=1, SSRC=1
// send SSRC=2, all other fields 0
constexpr uint8_t kRtcpReport[] = {
    0x80, 0xc9, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

TEST(RtpPacketType, InferRtpPacketType) {
  EXPECT_EQ(InferRtpPacketType(kPcmuFrame), RtpPacketType::kRtp);
  EXPECT_EQ(InferRtpPacketType(kRtcpReport), RtpPacketType::kRtcp);
  EXPECT_EQ(InferRtpPacketType(kInvalidPacket), RtpPacketType::kUnknown);
}

}  // namespace
}  // namespace webrtc
