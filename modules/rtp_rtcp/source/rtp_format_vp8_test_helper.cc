/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_format_vp8_test_helper.h"

#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;

constexpr RtpPacketToSend::ExtensionManager* kNoExtensions = nullptr;

// Payload descriptor
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |X|R|N|S|PartID | (REQUIRED)
//      +-+-+-+-+-+-+-+-+
// X:   |I|L|T|K|  RSV  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
//      |M| PictureID   |
// I:   +-+-+-+-+-+-+-+-+ (OPTIONAL)
//      |   PictureID   |
//      +-+-+-+-+-+-+-+-+
// L:   |   TL0PICIDX   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// T/K: |TID|Y| KEYIDX  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+

// First octet names.
int Bit(uint8_t byte, int position) {
  return (byte >> position) & 0x01;
}

// eXtesnsion
int BitX(rtc::ArrayView<const uint8_t> header) {
  return Bit(header[0], 7);
}
int ReservedBaseBit(rtc::ArrayView<const uint8_t> header) {
  return Bit(header[0], 6);
}
// iNdependent
int BitN(rtc::ArrayView<const uint8_t> header) {
  return Bit(header[0], 5);
}
// Start of partitition.
int BitS(rtc::ArrayView<const uint8_t> header) {
  return Bit(header[0], 4);
}
int PartId(rtc::ArrayView<const uint8_t> header) {
  return header[0] & 0x0f;
}

// pIcture
int BitI(rtc::ArrayView<const uint8_t> header) {
  return Bit(header[1], 7);
}
// base Layer
int BitL(rtc::ArrayView<const uint8_t> header) {
  return Bit(header[1], 6);
}
// Temporal id
int BitT(rtc::ArrayView<const uint8_t> header) {
  return Bit(header[1], 5);
}
// Key index
int BitK(rtc::ArrayView<const uint8_t> header) {
  return Bit(header[1], 4);
}
int ReservedExtendedBits(rtc::ArrayView<const uint8_t> header) {
  return header[1] & 0x07;
}

int Tid(uint8_t byte) {
  return (byte & 0xC0) >> 6;
}
int BitY(uint8_t byte) {
  return Bit(byte, 5);
}
int KeyIdx(uint8_t byte) {
  return byte & 0x1f;
}

}  // namespace

RtpFormatVp8TestHelper::RtpFormatVp8TestHelper(const RTPVideoHeaderVP8* hdr,
                                               size_t payload_len)
    : hdr_info_(hdr), payload_(payload_len) {
  for (size_t i = 0; i < payload_.size(); ++i) {
    payload_[i] = i;
  }
}

RtpFormatVp8TestHelper::~RtpFormatVp8TestHelper() = default;

void RtpFormatVp8TestHelper::GetAllPacketsAndCheck(
    RtpPacketizerVp8* packetizer,
    rtc::ArrayView<const size_t> expected_sizes) {
  EXPECT_EQ(packetizer->NumPackets(), expected_sizes.size());
  const uint8_t* data_ptr = payload_.begin();
  RtpPacketToSend packet(kNoExtensions);
  for (size_t i = 0; i < expected_sizes.size(); ++i) {
    EXPECT_TRUE(packetizer->NextPacket(&packet));
    auto rtp_payload = packet.payload();
    EXPECT_EQ(rtp_payload.size(), expected_sizes[i]);

    int payload_offset = CheckHeader(rtp_payload, /*first=*/i == 0);
    // Verify that the payload (i.e., after the headers) of the packet is
    // identical to the expected (as found in data_ptr).
    auto vp8_payload = rtp_payload.subview(payload_offset);
    EXPECT_THAT(vp8_payload, ElementsAreArray(data_ptr, vp8_payload.size()));
    data_ptr += vp8_payload.size();
    // Verify packetizer didn't put more payload bytes than requested.
    ASSERT_GE(payload_.end() - data_ptr, 0);
  }
  EXPECT_EQ(payload_.end() - data_ptr, 0);
}

int RtpFormatVp8TestHelper::CheckHeader(rtc::ArrayView<const uint8_t> buffer,
                                        bool first) {
  int payload_offset = 1;
  EXPECT_EQ(ReservedBaseBit(buffer), 0);
  EXPECT_EQ(PartId(buffer), 0);  // In equal size mode, PartID is always 0.

  if (hdr_info_->pictureId != kNoPictureId ||
      hdr_info_->temporalIdx != kNoTemporalIdx ||
      hdr_info_->tl0PicIdx != kNoTl0PicIdx || hdr_info_->keyIdx != kNoKeyIdx) {
    EXPECT_EQ(BitX(buffer), 1);
    EXPECT_EQ(ReservedExtendedBits(buffer), 0);
    ++payload_offset;
    CheckPictureID(buffer, &payload_offset);
    CheckTl0PicIdx(buffer, &payload_offset);
    CheckTIDAndKeyIdx(buffer, &payload_offset);
  } else {
    EXPECT_EQ(BitX(buffer), 0);
  }

  EXPECT_EQ(BitN(buffer), hdr_info_->nonReference ? 1 : 0);
  EXPECT_EQ(BitS(buffer), first ? 1 : 0);
  return payload_offset;
}

// Verify that the I bit and the PictureID field are both set in accordance
// with the information in hdr_info_->pictureId.
void RtpFormatVp8TestHelper::CheckPictureID(
    rtc::ArrayView<const uint8_t> buffer,
    int* offset) {
  if (hdr_info_->pictureId != kNoPictureId) {
    EXPECT_EQ(BitI(buffer), 1);
    int two_byte_picture_id = Bit(buffer[*offset], 7);
    EXPECT_EQ(two_byte_picture_id, 1);
    EXPECT_EQ(buffer[*offset] & 0x7F, (hdr_info_->pictureId >> 8) & 0x7F);
    EXPECT_EQ(buffer[(*offset) + 1], hdr_info_->pictureId & 0xFF);
    (*offset) += 2;
  } else {
    EXPECT_EQ(BitI(buffer), 0);
  }
}

// Verify that the L bit and the TL0PICIDX field are both set in accordance
// with the information in hdr_info_->tl0PicIdx.
void RtpFormatVp8TestHelper::CheckTl0PicIdx(
    rtc::ArrayView<const uint8_t> buffer,
    int* offset) {
  if (hdr_info_->tl0PicIdx != kNoTl0PicIdx) {
    EXPECT_EQ(BitL(buffer), 1);
    EXPECT_EQ(buffer[*offset], hdr_info_->tl0PicIdx);
    ++*offset;
  } else {
    EXPECT_EQ(BitL(buffer), 0);
  }
}

// Verify that the T bit and the TL0PICIDX field, and the K bit and KEYIDX
// field are all set in accordance with the information in
// hdr_info_->temporalIdx and hdr_info_->keyIdx, respectively.
void RtpFormatVp8TestHelper::CheckTIDAndKeyIdx(
    rtc::ArrayView<const uint8_t> buffer,
    int* offset) {
  if (hdr_info_->temporalIdx == kNoTemporalIdx &&
      hdr_info_->keyIdx == kNoKeyIdx) {
    EXPECT_EQ(BitT(buffer), 0);
    EXPECT_EQ(BitK(buffer), 0);
    return;
  }
  if (hdr_info_->temporalIdx != kNoTemporalIdx) {
    EXPECT_EQ(BitT(buffer), 1);
    EXPECT_EQ(Tid(buffer[*offset]), hdr_info_->temporalIdx);
    EXPECT_EQ(BitY(buffer[*offset]), hdr_info_->layerSync ? 1 : 0);
  } else {
    EXPECT_EQ(BitT(buffer), 0);
    EXPECT_EQ(Tid(buffer[*offset]), 0);
    EXPECT_EQ(BitY(buffer[*offset]), 0);
  }
  if (hdr_info_->keyIdx != kNoKeyIdx) {
    EXPECT_EQ(BitK(buffer), 1);
    EXPECT_EQ(KeyIdx(buffer[*offset]), hdr_info_->keyIdx);
  } else {
    EXPECT_EQ(BitK(buffer), 0);
    EXPECT_EQ(KeyIdx(buffer[*offset]), 0);
  }
  ++*offset;
}

}  // namespace webrtc
