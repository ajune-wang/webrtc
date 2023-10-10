/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packetizer_h265.h"

#include <vector>

#include "common_video/h265/h265_common.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr RtpPacketToSend::ExtensionManager* kNoExtensions = nullptr;
constexpr size_t kMaxPayloadSize = 1200;
constexpr size_t kLengthFieldLength = 2;
constexpr RtpPacketizer::PayloadSizeLimits kNoLimits;

constexpr size_t kNalHeaderSize = 2;
constexpr size_t kFuAHeaderSize = 3;

constexpr uint8_t kNaluTypeMask = 0x7E;

// Bit masks for FU (A and B) headers.
constexpr uint8_t kH265SBit = 0x80;
constexpr uint8_t kH265EBit = 0x40;

// Creates Buffer that looks like nal unit of given size.
rtc::Buffer GenerateNalUnit(size_t size) {
  RTC_CHECK_GT(size, 0);
  rtc::Buffer buffer(size);
  // Set some valid header with type TRAIL_R and temporal id
  buffer[0] = 2;
  buffer[1] = 2;
  for (size_t i = 2; i < size; ++i) {
    buffer[i] = static_cast<uint8_t>(i);
  }
  // Last byte shouldn't be 0, or it may be counted as part of next 4-byte start
  // sequence.
  buffer[size - 1] |= 0x10;
  return buffer;
}

// Create frame consisting of nalus of given size.
rtc::Buffer CreateFrame(std::initializer_list<size_t> nalu_sizes) {
  static constexpr int kStartCodeSize = 3;
  rtc::Buffer frame(absl::c_accumulate(nalu_sizes, size_t{0}) +
                    kStartCodeSize * nalu_sizes.size());
  size_t offset = 0;
  for (size_t nalu_size : nalu_sizes) {
    EXPECT_GE(nalu_size, 1u);
    // Insert nalu start code
    frame[offset] = 0;
    frame[offset + 1] = 0;
    frame[offset + 2] = 1;
    // Set some valid header.
    frame[offset + 3] = 2;
    // Fill payload avoiding accidental start codes
    if (nalu_size > 1) {
      memset(frame.data() + offset + 4, 0x3f, nalu_size - 1);
    }
    offset += (kStartCodeSize + nalu_size);
  }
  return frame;
}

// Create frame consisting of given nalus.
rtc::Buffer CreateFrame(rtc::ArrayView<const rtc::Buffer> nalus) {
  static constexpr int kStartCodeSize = 3;
  int frame_size = 0;
  for (const rtc::Buffer& nalu : nalus) {
    frame_size += (kStartCodeSize + nalu.size());
  }
  rtc::Buffer frame(frame_size);
  size_t offset = 0;
  for (const rtc::Buffer& nalu : nalus) {
    // Insert nalu start code
    frame[offset] = 0;
    frame[offset + 1] = 0;
    frame[offset + 2] = 1;
    // Copy the nalu unit.
    memcpy(frame.data() + offset + 3, nalu.data(), nalu.size());
    offset += (kStartCodeSize + nalu.size());
  }
  return frame;
}

std::vector<RtpPacketToSend> FetchAllPackets(RtpPacketizerH265* packetizer) {
  std::vector<RtpPacketToSend> result;
  size_t num_packets = packetizer->NumPackets();
  result.reserve(num_packets);
  RtpPacketToSend packet(kNoExtensions);
  while (packetizer->NextPacket(&packet)) {
    result.push_back(packet);
  }
  EXPECT_THAT(result, SizeIs(num_packets));
  return result;
}

// Single nalu tests.
TEST(RtpPacketizerH265Test, SingleNalu) {
  const uint8_t frame[] = {0, 0, 1, H265::kIdrWRadl, 0xFF};

  RtpPacketizerH265 packetizer(frame, kNoLimits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(1));
  EXPECT_THAT(packets[0].payload(), ElementsAre(H265::kIdrWRadl, 0xFF));
}

TEST(RtpPacketizerH265Test, SingleNaluTwoPackets) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = kMaxPayloadSize;
  rtc::Buffer nalus[] = {GenerateNalUnit(kMaxPayloadSize),
                         GenerateNalUnit(100)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(nalus[0]));
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[1]));
}

TEST(RtpPacketizerH265Test,
     SingleNaluFirstPacketReductionAppliesOnlyToFirstFragment) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 200;
  limits.first_packet_reduction_len = 5;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/195),
                         GenerateNalUnit(/*size=*/200),
                         GenerateNalUnit(/*size=*/200)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(3));
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(nalus[0]));
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[1]));
  EXPECT_THAT(packets[2].payload(), ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH265Test,
     SingleNaluLastPacketReductionAppliesOnlyToLastFragment) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 200;
  limits.last_packet_reduction_len = 5;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/200),
                         GenerateNalUnit(/*size=*/200),
                         GenerateNalUnit(/*size=*/195)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(3));
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(nalus[0]));
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[1]));
  EXPECT_THAT(packets[2].payload(), ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH265Test,
     SingleNaluFirstAndLastPacketReductionSumsForSinglePacket) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 200;
  limits.first_packet_reduction_len = 20;
  limits.last_packet_reduction_len = 30;
  rtc::Buffer frame = CreateFrame({150});

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  EXPECT_THAT(packets, SizeIs(1));
}

// Aggregation tests.
TEST(RtpPacketizerH265Test, AP) {
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/0x123)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, kNoLimits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(1));
  auto payload = packets[0].payload();
  int type = H265::ParseNaluType(payload[0]);
  EXPECT_EQ(payload.size(),
            kNalHeaderSize + 3 * kLengthFieldLength + 2 + 2 + 0x123);

  EXPECT_EQ(type, H265::NaluType::kAP);
  payload = payload.subview(kNalHeaderSize);
  // 1st fragment.
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0, 2));  // Size.
  EXPECT_THAT(payload.subview(kLengthFieldLength, 2),
              ElementsAreArray(nalus[0]));
  payload = payload.subview(kLengthFieldLength + 2);
  // 2nd fragment.
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0, 2));  // Size.
  EXPECT_THAT(payload.subview(kLengthFieldLength, 2),
              ElementsAreArray(nalus[1]));
  payload = payload.subview(kLengthFieldLength + 2);
  // 3rd fragment.
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0x1, 0x23));  // Size.
  EXPECT_THAT(payload.subview(kLengthFieldLength), ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH265Test, APRespectsFirstPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1000;
  limits.first_packet_reduction_len = 100;
  const size_t kFirstFragmentSize =
      limits.max_payload_len - limits.first_packet_reduction_len;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/kFirstFragmentSize),
                         GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/2)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  // Expect 1st packet is single nalu.
  EXPECT_THAT(packets[0].payload(), ElementsAreArray(nalus[0]));
  // Expect 2nd packet is aggregate of last two fragments.
  // The size of H265 nal_unit_header is 2 bytes, according to 7.3.1.2
  // in H265 spec. Aggregation packet type is 48, and nuh_temporal_id_plus1
  // is 2, so the nal_unit_header should be "01100000 00000010",
  // which is 96 and 2.
  EXPECT_THAT(packets[1].payload(),
              ElementsAre(96, 2,                           //
                          0, 2, nalus[1][0], nalus[1][1],  //
                          0, 2, nalus[2][0], nalus[2][1]));
}

TEST(RtpPacketizerH265Test, APRespectsLastPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1000;
  limits.last_packet_reduction_len = 100;
  const size_t kLastFragmentSize =
      limits.max_payload_len - limits.last_packet_reduction_len;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/kLastFragmentSize)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  // Expect 1st packet is aggregate of 1st two fragments.
  EXPECT_THAT(packets[0].payload(),
              ElementsAre(96, 2,                           //
                          0, 2, nalus[0][0], nalus[0][1],  //
                          0, 2, nalus[1][0], nalus[1][1]));
  // Expect 2nd packet is single nalu.
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH265Test, TooSmallForAPHeaders) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1000;
  const size_t kLastFragmentSize =
      limits.max_payload_len - 3 * kLengthFieldLength - 4;
  rtc::Buffer nalus[] = {GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/2),
                         GenerateNalUnit(/*size=*/kLastFragmentSize)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(2));
  // Expect 1st packet is aggregate of 1st two fragments.
  EXPECT_THAT(packets[0].payload(),
              ElementsAre(96, 2,                           //
                          0, 2, nalus[0][0], nalus[0][1],  //
                          0, 2, nalus[1][0], nalus[1][1]));
  // Expect 2nd packet is single nalu.
  EXPECT_THAT(packets[1].payload(), ElementsAreArray(nalus[2]));
}

// Fragmentation + aggregation.
TEST(RtpPacketizerH265Test, MixedAPFUA) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 100;
  const size_t kFuaPayloadSize = 70;
  const size_t kFuaNaluSize = kNalHeaderSize + 2 * kFuaPayloadSize;
  const size_t kStapANaluSize = 20;
  rtc::Buffer nalus[] = {GenerateNalUnit(kFuaNaluSize),
                         GenerateNalUnit(kStapANaluSize),
                         GenerateNalUnit(kStapANaluSize)};
  rtc::Buffer frame = CreateFrame(nalus);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  ASSERT_THAT(packets, SizeIs(3));
  // First expect two FU-A packets.
  // The size of H265 nal_unit_header is 2 bytes, according to 7.3.1.2
  // in H265 spec. Aggregation packet type is 49, and nuh_temporal_id_plus1
  // is 2, so the nal_unit_header should be "01100010 00000010",
  // which is 98 and 2.
  uint8_t nalu_type = (nalus[0][0] & kNaluTypeMask) >> 1;
  EXPECT_THAT(packets[0].payload().subview(0, kFuAHeaderSize),
              ElementsAre(98, 2, kH265SBit | nalu_type));
  EXPECT_THAT(
      packets[0].payload().subview(kFuAHeaderSize),
      ElementsAreArray(nalus[0].data() + kNalHeaderSize, kFuaPayloadSize));

  EXPECT_THAT(packets[1].payload().subview(0, kFuAHeaderSize),
              ElementsAre(98, 2, kH265EBit | nalu_type));
  EXPECT_THAT(
      packets[1].payload().subview(kFuAHeaderSize),
      ElementsAreArray(nalus[0].data() + kNalHeaderSize + kFuaPayloadSize,
                       kFuaPayloadSize));

  // Then expect one AP packet with two nal units.
  int type = H265::ParseNaluType(packets[2].payload()[0]);
  EXPECT_THAT(type, H265::NaluType::kAP);
  auto payload = packets[2].payload().subview(kNalHeaderSize);
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0, kStapANaluSize));
  EXPECT_THAT(payload.subview(kLengthFieldLength, kStapANaluSize),
              ElementsAreArray(nalus[1]));
  payload = payload.subview(kLengthFieldLength + kStapANaluSize);
  EXPECT_THAT(payload.subview(0, kLengthFieldLength),
              ElementsAre(0, kStapANaluSize));
  EXPECT_THAT(payload.subview(kLengthFieldLength), ElementsAreArray(nalus[2]));
}

TEST(RtpPacketizerH265Test, LastFragmentFitsInSingleButNotLastPacket) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1178;
  limits.first_packet_reduction_len = 0;
  limits.last_packet_reduction_len = 20;
  limits.single_packet_reduction_len = 20;
  // Actual sizes, which triggered this bug.
  rtc::Buffer frame = CreateFrame({20, 8, 18, 1161});

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  // Last packet has to be of correct size.
  // Incorrect implementation might miss this constraint and not split the last
  // fragment in two packets.
  EXPECT_LE(static_cast<int>(packets.back().payload_size()),
            limits.max_payload_len - limits.last_packet_reduction_len);
}

// Splits frame with payload size `frame_payload_size` without fragmentation,
// Returns sizes of the payloads excluding fua headers.
std::vector<int> TestFua(size_t frame_payload_size,
                         const RtpPacketizer::PayloadSizeLimits& limits) {
  rtc::Buffer nalu[] = {GenerateNalUnit(kNalHeaderSize + frame_payload_size)};
  rtc::Buffer frame = CreateFrame(nalu);

  RtpPacketizerH265 packetizer(frame, limits);
  std::vector<RtpPacketToSend> packets = FetchAllPackets(&packetizer);

  EXPECT_GE(packets.size(), 2u);  // Single packet indicates it is not FuA.
  std::vector<uint16_t> fua_header;
  std::vector<int> payload_sizes;

  for (const RtpPacketToSend& packet : packets) {
    auto payload = packet.payload();
    EXPECT_GT(payload.size(), kFuAHeaderSize);
    // FU header is after the 2-bytes size PayloadHdr according to 4.4.3 in spec
    fua_header.push_back(payload[2]);
    payload_sizes.push_back(payload.size() - kFuAHeaderSize);
  }

  EXPECT_TRUE(fua_header.front() & kH265SBit);
  EXPECT_TRUE(fua_header.back() & kH265EBit);
  // Clear S and E bits before testing all are duplicating same original header.
  fua_header.front() &= ~kH265SBit;
  fua_header.back() &= ~kH265EBit;
  uint8_t nalu_type = (nalu[0][0] & kNaluTypeMask) >> 1;
  EXPECT_THAT(fua_header, Each(Eq(nalu_type)));

  return payload_sizes;
}

// Fragmentation tests.
TEST(RtpPacketizerH265Test, FUAOddSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  EXPECT_THAT(TestFua(1200, limits), ElementsAre(600, 600));
}

TEST(RtpPacketizerH265Test, FUAWithFirstPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  limits.first_packet_reduction_len = 4;
  limits.single_packet_reduction_len = 4;
  EXPECT_THAT(TestFua(1198, limits), ElementsAre(597, 601));
}

TEST(RtpPacketizerH265Test, FUAWithLastPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  limits.last_packet_reduction_len = 4;
  limits.single_packet_reduction_len = 4;
  EXPECT_THAT(TestFua(1198, limits), ElementsAre(601, 597));
}

TEST(RtpPacketizerH265Test, FUAWithSinglePacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1199;
  limits.single_packet_reduction_len = 200;
  EXPECT_THAT(TestFua(1000, limits), ElementsAre(500, 500));
}

TEST(RtpPacketizerH265Test, FUAEvenSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  EXPECT_THAT(TestFua(1201, limits), ElementsAre(600, 601));
}

TEST(RtpPacketizerH265Test, FUARounding) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1448;
  EXPECT_THAT(TestFua(10123, limits),
              ElementsAre(1265, 1265, 1265, 1265, 1265, 1266, 1266, 1266));
}

TEST(RtpPacketizerH265Test, FUABig) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  // Generate 10 full sized packets, leave room for FU-A headers.
  EXPECT_THAT(
      TestFua(10 * (1200 - kFuAHeaderSize), limits),
      ElementsAre(1197, 1197, 1197, 1197, 1197, 1197, 1197, 1197, 1197, 1197));
}
}  // namespace
}  // namespace webrtc
