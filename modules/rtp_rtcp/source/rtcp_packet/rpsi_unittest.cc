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

#include "test/gmock.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

using ::testing::ElementsAreArray;
using ::testing::make_tuple;
using webrtc::rtcp::Rpsi;

namespace webrtc {
namespace {
constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint32_t kRemoteSsrc = 0x23456789;
constexpr uint32_t kPictureOrderCnt = 0x3334;
constexpr uint8_t kPayloadType = 100;
constexpr uint8_t kLayerId = 1;

// Manually created Rpsi packet matching constants above.
constexpr uint8_t kPacket[] = {0x83, 206,  0x00, 0x04, 0x12, 0x34, 0x56,
                               0x78, 0x23, 0x45, 0x67, 0x89, 0,    100,
                               48,   49,   0x00, 0x00, 0x33, 0x34};
}  // namespace

TEST(RtcpPacketRpsiTest, Parse) {
  Rpsi mutable_parsed;
  EXPECT_TRUE(test::ParseSinglePacket(kPacket, &mutable_parsed));
  const Rpsi& parsed = mutable_parsed;  // Read values from constant object.

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parsed.media_ssrc());
  EXPECT_EQ(kPayloadType, parsed.payload_type());
  EXPECT_EQ(kLayerId, parsed.layer_id());
  EXPECT_EQ(kPictureOrderCnt, parsed.picture_order_cnt());
}

TEST(RtcpPacketRpsiTest, Create) {
  Rpsi rpsi;
  rpsi.SetSenderSsrc(kSenderSsrc);
  rpsi.SetMediaSsrc(kRemoteSsrc);
  rpsi.SetLayerId(kLayerId);
  rpsi.SetPayloadType(kPayloadType);
  rpsi.SetPictureOrderCnt(kPictureOrderCnt);

  rtc::Buffer packet = rpsi.Build();

  EXPECT_THAT(make_tuple(packet.data(), packet.size()),
              ElementsAreArray(kPacket));
}

TEST(RtcpPacketRpsiTest, ParseFailsOnTooSmallPacket) {
  const uint8_t kTooSmallPacket[] = {0x83, 206,  0x00, 0x01,
                                     0x12, 0x34, 0x56, 0x78};

  Rpsi parsed;
  EXPECT_FALSE(test::ParseSinglePacket(kTooSmallPacket, &parsed));
}

TEST(RtcpPacketRpsiTest, ParseFailsOnWrongPaddingBits) {
  Rpsi rpsi;
  rpsi.SetSenderSsrc(kSenderSsrc);
  rpsi.SetMediaSsrc(kRemoteSsrc);
  rpsi.SetLayerId(kLayerId);
  rpsi.SetPayloadType(kPayloadType);
  rpsi.SetPictureOrderCnt(kPictureOrderCnt);

  rtc::Buffer packet = rpsi.Build();
  uint8_t* padding_bits = packet.data() + 12;
  ASSERT_TRUE(test::ParseSinglePacket(packet, &rpsi));
  *padding_bits += 8;
  EXPECT_FALSE(test::ParseSinglePacket(packet, &rpsi));
}

}  // namespace webrtc
