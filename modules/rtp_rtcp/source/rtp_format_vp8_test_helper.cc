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

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
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
// I:   |   PictureID   | (OPTIONAL)
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
// Reserved
int BitR(rtc::ArrayView<const uint8_t> header) {
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
    : hdr_info_(hdr),
      payload_(payload_len),
      packet_(kNoExtensions),
      payload_start_(0) {
  for (size_t i = 0; i < payload_.size(); ++i) {
    payload_[i] = i;
  }
  data_ptr_ = payload_.begin();
}

RtpFormatVp8TestHelper::~RtpFormatVp8TestHelper() = default;

void RtpFormatVp8TestHelper::GetAllPacketsAndCheck(
    RtpPacketizerVp8* packetizer,
    rtc::ArrayView<const size_t> expected_sizes) {
  EXPECT_EQ(packetizer->NumPackets(), expected_sizes.size());
  for (size_t i = 0; i < expected_sizes.size(); ++i) {
    EXPECT_TRUE(packetizer->NextPacket(&packet_));
    EXPECT_EQ(packet_.payload_size(), expected_sizes[i]);
    CheckHeader(/*first=*/i == 0);
    CheckPayload();
    CheckLast(/*last=*/i == expected_sizes.size() - 1);
  }
}

void RtpFormatVp8TestHelper::CheckHeader(bool first) {
  payload_start_ = 1;
  rtc::ArrayView<const uint8_t> buffer = packet_.payload();
  EXPECT_EQ(BitR(buffer), 0);    // Check reserved bit.
  EXPECT_EQ(PartId(buffer), 0);  // In equal size mode, PartID is always 0.

  if (hdr_info_->pictureId != kNoPictureId ||
      hdr_info_->temporalIdx != kNoTemporalIdx ||
      hdr_info_->tl0PicIdx != kNoTl0PicIdx || hdr_info_->keyIdx != kNoKeyIdx) {
    EXPECT_EQ(BitX(buffer), 1);
    ++payload_start_;
    CheckPictureID();
    CheckTl0PicIdx();
    CheckTIDAndKeyIdx();
  } else {
    EXPECT_EQ(BitX(buffer), 0);
  }

  EXPECT_EQ(BitN(buffer), hdr_info_->nonReference ? 1 : 0);
  EXPECT_EQ(BitS(buffer), first ? 1 : 0);
}

// Verify that the I bit and the PictureID field are both set in accordance
// with the information in hdr_info_->pictureId.
void RtpFormatVp8TestHelper::CheckPictureID() {
  auto buffer = packet_.payload();
  if (hdr_info_->pictureId != kNoPictureId) {
    EXPECT_EQ(BitI(buffer), 1);
    int two_byte_picture_id = Bit(buffer[payload_start_], 7);
    EXPECT_EQ(two_byte_picture_id, 1);
    EXPECT_EQ(buffer[payload_start_] & 0x7F,
              (hdr_info_->pictureId >> 8) & 0x7F);
    EXPECT_EQ(buffer[payload_start_ + 1], hdr_info_->pictureId & 0xFF);
    payload_start_ += 2;
  } else {
    EXPECT_EQ(BitI(buffer), 0);
  }
}

// Verify that the L bit and the TL0PICIDX field are both set in accordance
// with the information in hdr_info_->tl0PicIdx.
void RtpFormatVp8TestHelper::CheckTl0PicIdx() {
  auto buffer = packet_.payload();
  if (hdr_info_->tl0PicIdx != kNoTl0PicIdx) {
    EXPECT_EQ(BitL(buffer), 1);
    EXPECT_EQ(buffer[payload_start_], hdr_info_->tl0PicIdx);
    ++payload_start_;
  } else {
    EXPECT_EQ(BitL(buffer), 0);
  }
}

// Verify that the T bit and the TL0PICIDX field, and the K bit and KEYIDX
// field are all set in accordance with the information in
// hdr_info_->temporalIdx and hdr_info_->keyIdx, respectively.
void RtpFormatVp8TestHelper::CheckTIDAndKeyIdx() {
  auto buffer = packet_.payload();
  if (hdr_info_->temporalIdx == kNoTemporalIdx &&
      hdr_info_->keyIdx == kNoKeyIdx) {
    EXPECT_EQ(BitT(buffer), 0);
    EXPECT_EQ(BitK(buffer), 0);
    return;
  }
  if (hdr_info_->temporalIdx != kNoTemporalIdx) {
    EXPECT_EQ(BitT(buffer), 1);
    EXPECT_EQ(Tid(buffer[payload_start_]), hdr_info_->temporalIdx);
    EXPECT_EQ(BitY(buffer[payload_start_]), hdr_info_->layerSync ? 1 : 0);
  } else {
    EXPECT_EQ(BitT(buffer), 0);
    EXPECT_EQ(Tid(buffer[payload_start_]), 0);
    EXPECT_EQ(BitY(buffer[payload_start_]), 0);
  }
  if (hdr_info_->keyIdx != kNoKeyIdx) {
    EXPECT_EQ(BitK(buffer), 1);
    EXPECT_EQ(KeyIdx(buffer[payload_start_]), hdr_info_->keyIdx);
  } else {
    EXPECT_EQ(BitK(buffer), 0);
    EXPECT_EQ(KeyIdx(buffer[payload_start_]), 0);
  }
  ++payload_start_;
}

// Verify that the payload (i.e., after the headers) of the packet stored in
// buffer_ is identical to the expected (as found in data_ptr_).
void RtpFormatVp8TestHelper::CheckPayload() {
  int vp8_payload_size = packet_.payload_size() - payload_start_;
  EXPECT_THAT(packet_.payload().subview(payload_start_),
              ElementsAreArray(data_ptr_, vp8_payload_size));
  data_ptr_ += vp8_payload_size;
}

// Verify that the input variable "last" agrees with the position of data_ptr_.
// If data_ptr_ has advanced payload_size() bytes from the start
// payload_.begin() we are at the end and last should be true. Otherwise, it
// should be false.
void RtpFormatVp8TestHelper::CheckLast(bool last) const {
  EXPECT_EQ(last, data_ptr_ == payload_.end());
}

}  // namespace test

}  // namespace webrtc
