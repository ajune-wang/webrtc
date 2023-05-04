/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/flexfec_header_reader_writer2.h"

#include <string.h>

#include <memory>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/make_ref_counted.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using Packet = ForwardErrorCorrection::Packet;
using ProtectedStream = ForwardErrorCorrection::ProtectedStream;
using ReceivedFecPacket = ForwardErrorCorrection::ReceivedFecPacket;
using ::testing::ElementsAreArray;
using ::testing::make_tuple;
using ::testing::SizeIs;

constexpr size_t kFlexfecPacketMaskSizes[] = {2, 6, 14};
constexpr size_t kFlexfecMaxMaskSize = kFlexfecPacketMaskSizes[2];

// Reader tests.
constexpr uint8_t kNoRBit = 0 << 7;
constexpr uint8_t kNoFBit = 0 << 6;
constexpr uint8_t kPtRecovery = 123;
constexpr uint8_t kLengthRecov[] = {0xab, 0xcd};
constexpr uint8_t kTsRecovery[] = {0x01, 0x23, 0x45, 0x67};
constexpr uint8_t kSnBases[4][2] = {{0x01, 0x02},
                                    {0x03, 0x04},
                                    {0x05, 0x06},
                                    {0x07, 0x08}};
constexpr uint8_t kPayloadBits = 0x00;

struct FecPacketStreamProperties {
  ProtectedStream stream;
  rtc::ArrayView<const uint8_t> mask;
};

ProtectedStream CreateStreamProps(uint32_t ssrc,
                                  int sn_base_index,
                                  size_t packet_mask_offset,
                                  int mask_size_index) {
  return {.ssrc = ssrc,
          .seq_num_base =
              ByteReader<uint16_t>::ReadBigEndian(kSnBases[sn_base_index]),
          .packet_mask_offset = packet_mask_offset,
          .packet_mask_size = kFlexfecPacketMaskSizes[mask_size_index]};
}

std::unique_ptr<uint8_t[]> GeneratePacketMask(size_t packet_mask_size,
                                              uint64_t seed) {
  Random random(seed);
  std::unique_ptr<uint8_t[]> packet_mask(new uint8_t[kFlexfecMaxMaskSize]);
  memset(packet_mask.get(), 0, kFlexfecMaxMaskSize);
  for (size_t i = 0; i < packet_mask_size; ++i) {
    packet_mask[i] = random.Rand<uint8_t>();
  }
  return packet_mask;
}

void ClearBit(size_t index, uint8_t* packet_mask) {
  packet_mask[index / 8] &= ~(1 << (7 - index % 8));
}

void SetBit(size_t index, uint8_t* packet_mask) {
  packet_mask[index / 8] |= (1 << (7 - index % 8));
}

void VerifyReadHeaders(size_t expected_fec_header_size,
                       std::vector<const FecPacketStreamProperties> expected,
                       const ReceivedFecPacket& read_packet) {
  EXPECT_EQ(read_packet.fec_header_size, expected_fec_header_size);
  const size_t protected_streams_num = read_packet.protected_streams.size();
  EXPECT_EQ(protected_streams_num, expected.size());
  for (size_t i = 0; i < protected_streams_num; ++i) {
    ProtectedStream protected_stream = read_packet.protected_streams[i];
    EXPECT_EQ(protected_stream.ssrc, expected[i].stream.ssrc);
    EXPECT_EQ(protected_stream.seq_num_base, expected[i].stream.seq_num_base);
    EXPECT_EQ(protected_stream.packet_mask_offset,
              expected[i].stream.packet_mask_offset);
    EXPECT_EQ(protected_stream.packet_mask_size,
              expected[i].stream.packet_mask_size);
    // Ensure that the K-bits are removed and the packet mask has been packed.
    EXPECT_THAT(
        rtc::ArrayView<const uint8_t>(
            read_packet.pkt->data.cdata() + protected_stream.packet_mask_offset,
            protected_stream.packet_mask_size),
        ::testing::ElementsAreArray(expected[i].mask));
  }
  EXPECT_EQ(read_packet.pkt->data.size() - expected_fec_header_size,
            read_packet.protection_length);
}

}  // namespace

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit0SetSingleStream) {
  constexpr uint8_t kKBit0 = 1 << 7;
  constexpr size_t kExpectedFecHeaderSize = 12;
  constexpr uint8_t kFlexfecPktMasks[] = {kKBit0 | 0x08, 0x81};
  constexpr uint8_t kUlpfecPacketMasks[] = {0x11, 0x02};
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit, kPtRecovery,         kLengthRecov[0],
      kLengthRecov[1],   kTsRecovery[0],      kTsRecovery[1],
      kTsRecovery[2],    kTsRecovery[3],      kSnBases[0][0],
      kSnBases[0][1],    kFlexfecPktMasks[0], kFlexfecPktMasks[1],
      kPayloadBits,      kPayloadBits,        kPayloadBits,
      kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<const FecPacketStreamProperties> expected = {
      {.stream = CreateStreamProps(0x01, 0, 10, 0),
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMasks,
                                             kFlexfecPacketMaskSizes[0])}};

  VerifyReadHeaders(kExpectedFecHeaderSize, expected, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit1SetSingleStream) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 1 << 7;
  constexpr size_t kExpectedFecHeaderSize = 16;
  constexpr uint8_t kFlexfecPktMasks[] = {kKBit0 | 0x48, 0x81,  //
                                          kKBit1 | 0x02, 0x11, 0x00, 0x21};
  constexpr uint8_t kUlpfecPacketMasks[] = {0x91, 0x02,  //
                                            0x08, 0x44, 0x00, 0x84};
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit,   kPtRecovery,         kLengthRecov[0],
      kLengthRecov[1],     kTsRecovery[0],      kTsRecovery[1],
      kTsRecovery[2],      kTsRecovery[3],      kSnBases[0][0],
      kSnBases[0][1],      kFlexfecPktMasks[0], kFlexfecPktMasks[1],
      kFlexfecPktMasks[2], kFlexfecPktMasks[3], kFlexfecPktMasks[4],
      kFlexfecPktMasks[5], kPayloadBits,        kPayloadBits,
      kPayloadBits,        kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<const FecPacketStreamProperties> expected = {
      {.stream = {CreateStreamProps(0x01, 0, 10, 1)},
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMasks,
                                             kFlexfecPacketMaskSizes[1])}};

  VerifyReadHeaders(kExpectedFecHeaderSize, expected, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithNoKBitsSetSingleStream) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 0 << 7;
  constexpr size_t kExpectedFecHeaderSize = 24;
  constexpr uint8_t kFlxfcPktMask[] = {kKBit0 | 0x48, 0x81,              //
                                       kKBit1 | 0x02, 0x11, 0x00, 0x21,  //
                                       0x01,          0x11, 0x11, 0x11,
                                       0x11,          0x11, 0x11, 0x11};
  constexpr uint8_t kUlpfecPacketMasks[] = {0x91, 0x02,              //
                                            0x08, 0x44, 0x00, 0x84,  //
                                            0x04, 0x44, 0x44, 0x44,
                                            0x44, 0x44, 0x44, 0x44};
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit, kPtRecovery,       kLengthRecov[0],
      kLengthRecov[1],   kTsRecovery[0],    kTsRecovery[1],
      kTsRecovery[2],    kTsRecovery[3],    kSnBases[0][0],
      kSnBases[0][1],    kFlxfcPktMask[0],  kFlxfcPktMask[1],
      kFlxfcPktMask[2],  kFlxfcPktMask[3],  kFlxfcPktMask[4],
      kFlxfcPktMask[5],  kFlxfcPktMask[6],  kFlxfcPktMask[7],
      kFlxfcPktMask[8],  kFlxfcPktMask[9],  kFlxfcPktMask[10],
      kFlxfcPktMask[11], kFlxfcPktMask[12], kFlxfcPktMask[13],
      kPayloadBits,      kPayloadBits,      kPayloadBits,
      kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<const FecPacketStreamProperties> expected = {
      {.stream = CreateStreamProps(0x01, 0, 10, 2),
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMasks,
                                             kFlexfecPacketMaskSizes[2])}};

  VerifyReadHeaders(kExpectedFecHeaderSize, expected, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit0Set2Streams) {
  constexpr uint8_t kKBit0 = 1 << 7;
  constexpr size_t kExpectedFecHeaderSize = 16;
  constexpr uint8_t kFlexfecPktMask1[] = {kKBit0 | 0x08, 0x81};
  constexpr uint8_t kUlpfecPacketMask1[] = {0x11, 0x02};
  constexpr uint8_t kFlexfecPktMask2[] = {kKBit0 | 0x04, 0x41};
  constexpr uint8_t kUlpfecPacketMask2[] = {0x08, 0x82};

  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit,   kPtRecovery,         kLengthRecov[0],
      kLengthRecov[1],     kTsRecovery[0],      kTsRecovery[1],
      kTsRecovery[2],      kTsRecovery[3],      kSnBases[0][0],
      kSnBases[0][1],      kFlexfecPktMask1[0], kFlexfecPktMask1[1],
      kSnBases[1][0],      kSnBases[1][1],      kFlexfecPktMask2[0],
      kFlexfecPktMask2[1], kPayloadBits,        kPayloadBits,
      kPayloadBits,        kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_streams = {{.ssrc = 0x01}, {.ssrc = 0x02}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<const FecPacketStreamProperties> expected = {
      {.stream = CreateStreamProps(0x01, 0, 10, 0),
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMask1,
                                             kFlexfecPacketMaskSizes[0])},
      {.stream = CreateStreamProps(0x02, 1, 14, 0),
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMask2,
                                             kFlexfecPacketMaskSizes[0])},
  };

  VerifyReadHeaders(kExpectedFecHeaderSize, expected, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit1Set2Streams) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 1 << 7;
  constexpr size_t kExpectedFecHeaderSize = 24;
  constexpr uint8_t kFlexfecPktMask1[] = {kKBit0 | 0x48, 0x81,  //
                                          kKBit1 | 0x02, 0x11, 0x00, 0x21};
  constexpr uint8_t kUlpfecPacketMask1[] = {0x91, 0x02,  //
                                            0x08, 0x44, 0x00, 0x84};
  constexpr uint8_t kFlexfecPktMask2[] = {kKBit0 | 0x57, 0x82,  //
                                          kKBit1 | 0x04, 0x33, 0x00, 0x51};
  constexpr uint8_t kUlpfecPacketMask2[] = {0xAF, 0x04,  //
                                            0x10, 0xCC, 0x01, 0x44};
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit,   kPtRecovery,         kLengthRecov[0],
      kLengthRecov[1],     kTsRecovery[0],      kTsRecovery[1],
      kTsRecovery[2],      kTsRecovery[3],      kSnBases[0][0],
      kSnBases[0][1],      kFlexfecPktMask1[0], kFlexfecPktMask1[1],
      kFlexfecPktMask1[2], kFlexfecPktMask1[3], kFlexfecPktMask1[4],
      kFlexfecPktMask1[5], kSnBases[1][0],      kSnBases[1][1],
      kFlexfecPktMask2[0], kFlexfecPktMask2[1], kFlexfecPktMask2[2],
      kFlexfecPktMask2[3], kFlexfecPktMask2[4], kFlexfecPktMask2[5],
      kPayloadBits,        kPayloadBits,        kPayloadBits,
      kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_streams = {{.ssrc = 0x01}, {.ssrc = 0x02}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<const FecPacketStreamProperties> expected = {
      {.stream = CreateStreamProps(0x01, 0, 10, 1),
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMask1,
                                             kFlexfecPacketMaskSizes[1])},
      {.stream = CreateStreamProps(0x02, 1, 18, 1),
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMask2,
                                             kFlexfecPacketMaskSizes[1])},
  };

  VerifyReadHeaders(kExpectedFecHeaderSize, expected, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithNoKBitsSet2Streams) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 0 << 7;
  constexpr size_t kExpectedFecHeaderSize = 40;
  constexpr uint8_t kFlexfecPktMask1[] = {kKBit0 | 0x48, 0x81,              //
                                          kKBit1 | 0x02, 0x11, 0x00, 0x21,  //
                                          0x01,          0x11, 0x11, 0x11,
                                          0x11,          0x11, 0x11, 0x11};
  constexpr uint8_t kUlpfecPacketMask1[] = {0x91, 0x02,              //
                                            0x08, 0x44, 0x00, 0x84,  //
                                            0x04, 0x44, 0x44, 0x44,
                                            0x44, 0x44, 0x44, 0x44};
  constexpr uint8_t kFlexfecPktMask2[] = {kKBit0 | 0x32, 0x84,              //
                                          kKBit1 | 0x05, 0x23, 0x00, 0x55,  //
                                          0xA3,          0x22, 0x22, 0x22,
                                          0x22,          0x22, 0x22, 0x35};
  constexpr uint8_t kUlpfecPacketMask2[] = {0x65, 0x08,              //
                                            0x14, 0x8C, 0x01, 0x56,  //
                                            0x8C, 0x88, 0x88, 0x88,
                                            0x88, 0x88, 0x88, 0xD4};

  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit,    kPtRecovery,          kLengthRecov[0],
      kLengthRecov[1],      kTsRecovery[0],       kTsRecovery[1],
      kTsRecovery[2],       kTsRecovery[3],       kSnBases[0][0],
      kSnBases[0][1],       kFlexfecPktMask1[0],  kFlexfecPktMask1[1],
      kFlexfecPktMask1[2],  kFlexfecPktMask1[3],  kFlexfecPktMask1[4],
      kFlexfecPktMask1[5],  kFlexfecPktMask1[6],  kFlexfecPktMask1[7],
      kFlexfecPktMask1[8],  kFlexfecPktMask1[9],  kFlexfecPktMask1[10],
      kFlexfecPktMask1[11], kFlexfecPktMask1[12], kFlexfecPktMask1[13],
      kSnBases[1][0],       kSnBases[1][1],       kFlexfecPktMask2[0],
      kFlexfecPktMask2[1],  kFlexfecPktMask2[2],  kFlexfecPktMask2[3],
      kFlexfecPktMask2[4],  kFlexfecPktMask2[5],  kFlexfecPktMask2[6],
      kFlexfecPktMask2[7],  kFlexfecPktMask2[8],  kFlexfecPktMask2[9],
      kFlexfecPktMask2[10], kFlexfecPktMask2[11], kFlexfecPktMask2[12],
      kFlexfecPktMask2[13], kPayloadBits,         kPayloadBits,
      kPayloadBits,         kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_streams = {{.ssrc = 0x01}, {.ssrc = 0x02}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<const FecPacketStreamProperties> expected = {
      {.stream = CreateStreamProps(0x01, 0, 10, 2),
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMask1,
                                             kFlexfecPacketMaskSizes[2])},
      {.stream = CreateStreamProps(0x02, 1, 26, 2),
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMask2,
                                             kFlexfecPacketMaskSizes[2])},
  };

  VerifyReadHeaders(kExpectedFecHeaderSize, expected, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithMultipleStreamsMultipleMasks) {
  constexpr uint8_t kBit0 = 0 << 7;
  constexpr uint8_t kBit1 = 1 << 7;
  constexpr size_t kExpectedFecHeaderSize = 44;
  constexpr uint8_t kFlxfcPktMsks[4][14] = {
      {kBit1 | 0x29, 0x91},
      {kBit0 | 0x32, 0xA1,  //
       kBit1 | 0x02, 0x11, 0x00, 0x21},
      {kBit0 | 0x48, 0x81,              //
       kBit0 | 0x02, 0x11, 0x00, 0x21,  //
       0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
      {kBit0 | 0x32, 0x84,  //
       kBit1 | 0x05, 0x23, 0x00, 0x55}};
  constexpr uint8_t kUlpfecPacketMasks[4][14] = {
      {0x53, 0x22},
      {0x65, 0x42,  //
       0x08, 0x44, 0x00, 0x84},
      {0x91, 0x02,              //
       0x08, 0x44, 0x00, 0x84,  //
       0x04, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44},
      {0x65, 0x08,  //
       0x14, 0x8C, 0x01, 0x54}};
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit,    kPtRecovery,          kLengthRecov[0],
      kLengthRecov[1],      kTsRecovery[0],       kTsRecovery[1],
      kTsRecovery[2],       kTsRecovery[3],       kSnBases[0][0],
      kSnBases[0][1],       kFlxfcPktMsks[0][0],  kFlxfcPktMsks[0][1],
      kSnBases[1][0],       kSnBases[1][1],       kFlxfcPktMsks[1][0],
      kFlxfcPktMsks[1][1],  kFlxfcPktMsks[1][2],  kFlxfcPktMsks[1][3],
      kFlxfcPktMsks[1][4],  kFlxfcPktMsks[1][5],  kSnBases[2][0],
      kSnBases[2][1],       kFlxfcPktMsks[2][0],  kFlxfcPktMsks[2][1],
      kFlxfcPktMsks[2][2],  kFlxfcPktMsks[2][3],  kFlxfcPktMsks[2][4],
      kFlxfcPktMsks[2][5],  kFlxfcPktMsks[2][6],  kFlxfcPktMsks[2][7],
      kFlxfcPktMsks[2][8],  kFlxfcPktMsks[2][9],  kFlxfcPktMsks[2][10],
      kFlxfcPktMsks[2][11], kFlxfcPktMsks[2][12], kFlxfcPktMsks[2][13],
      kSnBases[3][0],       kSnBases[3][1],       kFlxfcPktMsks[3][0],
      kFlxfcPktMsks[3][1],  kFlxfcPktMsks[3][2],  kFlxfcPktMsks[3][3],
      kFlxfcPktMsks[3][4],  kFlxfcPktMsks[3][5],  kPayloadBits,
      kPayloadBits,         kPayloadBits,         kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_streams = {
      {.ssrc = 0x01}, {.ssrc = 0x02}, {.ssrc = 0x03}, {.ssrc = 0x04}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<const FecPacketStreamProperties> expected = {
      {.stream = CreateStreamProps(0x01, 0, 10, 0),
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMasks[0],
                                             kFlexfecPacketMaskSizes[0])},
      {.stream = CreateStreamProps(0x02, 1, 14, 1),
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMasks[1],
                                             kFlexfecPacketMaskSizes[1])},
      {.stream = CreateStreamProps(0x03, 2, 22, 2),
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMasks[2],
                                             kFlexfecPacketMaskSizes[2])},
      {.stream = CreateStreamProps(0x04, 3, 38, 1),
       .mask = rtc::ArrayView<const uint8_t>(kUlpfecPacketMasks[3],
                                             kFlexfecPacketMaskSizes[1])},
  };

  VerifyReadHeaders(kExpectedFecHeaderSize, expected, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadPacketWithoutProtectedSsrcsShouldFail) {
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit, kPtRecovery,    kLengthRecov[0], kLengthRecov[1],
      kTsRecovery[0],    kTsRecovery[1], kTsRecovery[2],  kTsRecovery[3]};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  // No protected ssrcs.
  read_packet.protected_streams = {};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReader2Test,
     ReadPacketWithoutStreamSpecificHeaderShouldFail) {
  // Simulate short received packet.
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit, kPtRecovery,    kLengthRecov[0], kLengthRecov[1],
      kTsRecovery[0],    kTsRecovery[1], kTsRecovery[2],  kTsRecovery[3]};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReader2Test, ReadShortPacketWithKBit0SetShouldFail) {
  // Simulate short received packet.
  const size_t packet_mask_size = kFlexfecPacketMaskSizes[0];
  std::unique_ptr<uint8_t[]> packet_mask =
      GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(0, packet_mask.get());  // Set kbit0.
  uint8_t kPacketData[] = {kNoRBit | kNoFBit, kPtRecovery,    kLengthRecov[0],
                           kLengthRecov[1],   kTsRecovery[0], kTsRecovery[1],
                           kTsRecovery[2],    kTsRecovery[3], kSnBases[0][0],
                           kSnBases[0][1],    packet_mask[0], packet_mask[1]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  // Expected to have 2 bytes of mask but length of packet misses 1 byte.
  read_packet.pkt->data.SetData(kPacketData, sizeof(kPacketData) - 1);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReader2Test, ReadShortPacketWithKBit1SetShouldFail) {
  // Simulate short received packet.
  const size_t packet_mask_size = kFlexfecPacketMaskSizes[1];
  std::unique_ptr<uint8_t[]> packet_mask =
      GeneratePacketMask(packet_mask_size, 0xabcd);
  // Clear kbit0 and set kbit1.
  ClearBit(0, packet_mask.get());
  SetBit(16, packet_mask.get());
  uint8_t kPacketData[] = {
      kNoRBit | kNoFBit, kPtRecovery,    kLengthRecov[0], kLengthRecov[1],
      kTsRecovery[0],    kTsRecovery[1], kTsRecovery[2],  kTsRecovery[3],
      kSnBases[0][0],    kSnBases[0][1], packet_mask[0],  packet_mask[1],
      packet_mask[2],    packet_mask[3], packet_mask[4],  packet_mask[5]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  // Expected to have 6 bytes of mask but length of packet misses 2 bytes.
  read_packet.pkt->data.SetData(kPacketData, sizeof(kPacketData) - 2);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReader2Test, ReadShortPacketWithKBit1ClearedShouldFail) {
  // Simulate short received packet.
  const size_t packet_mask_size = kFlexfecPacketMaskSizes[2];
  std::unique_ptr<uint8_t[]> packet_mask =
      GeneratePacketMask(packet_mask_size, 0xabcd);
  // Clear kbit0 and kbit1.
  ClearBit(0, packet_mask.get());
  ClearBit(16, packet_mask.get());
  uint8_t kPacketData[] = {
      kNoRBit | kNoFBit, kPtRecovery,     kLengthRecov[0], kLengthRecov[1],
      kTsRecovery[0],    kTsRecovery[1],  kTsRecovery[2],  kTsRecovery[3],
      kSnBases[0][0],    kSnBases[0][1],  packet_mask[0],  packet_mask[1],
      packet_mask[2],    packet_mask[3],  packet_mask[4],  packet_mask[5],
      packet_mask[6],    packet_mask[7],  packet_mask[8],  packet_mask[9],
      packet_mask[10],   packet_mask[11], packet_mask[12], packet_mask[13]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  // Expected to have 14 bytes of mask but length of packet misses 2 bytes.
  read_packet.pkt->data.SetData(kPacketData, sizeof(kPacketData) - 2);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReader2Test, ReadShortPacketMultipleStreamsShouldFail) {
  // Simulate short received packet with 2 protected ssrcs.
  std::unique_ptr<uint8_t[]> packet_mask_1 =
      GeneratePacketMask(kFlexfecPacketMaskSizes[0], 0xabcd);
  std::unique_ptr<uint8_t[]> packet_mask_2 =
      GeneratePacketMask(kFlexfecPacketMaskSizes[2], 0xabcd);
  SetBit(0, packet_mask_1.get());
  ClearBit(0, packet_mask_2.get());
  ClearBit(16, packet_mask_2.get());
  uint8_t kPacketData[] = {
      kNoRBit | kNoFBit, kPtRecovery,       kLengthRecov[0],
      kLengthRecov[1],   kTsRecovery[0],    kTsRecovery[1],
      kTsRecovery[2],    kTsRecovery[3],    kSnBases[0][0],
      kSnBases[0][1],    packet_mask_1[0],  packet_mask_1[1],
      kSnBases[1][0],    kSnBases[1][1],    packet_mask_2[0],
      packet_mask_2[1],  packet_mask_2[2],  packet_mask_2[3],
      packet_mask_2[4],  packet_mask_2[5],  packet_mask_2[6],
      packet_mask_2[7],  packet_mask_2[8],  packet_mask_2[9],
      packet_mask_2[10], packet_mask_2[11], packet_mask_2[12],
      packet_mask_2[13]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  // Subtract 2 bytes from length, so the read will fail on parsing second
  read_packet.pkt->data.SetData(kPacketData, sizeof(kPacketData) - 2);
  read_packet.protected_streams = {{.ssrc = 0x01}, {.ssrc = 0x02}};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

// TODO(bugs.webrtc.org/15002): reimplement and add tests for multi stream cases
// after updating the Writer code.

TEST(FlexfecHeaderWriter2Test, FinalizesHeaderWithKBit0Set) {}

TEST(FlexfecHeaderWriter2Test, FinalizesHeaderWithKBit1Set) {}

TEST(FlexfecHeaderWriter2Test, FinalizesHeaderWithKBit2Set) {}

TEST(FlexfecHeaderWriter2Test, ContractsShortUlpfecPacketMaskWithBit15Clear) {}

TEST(FlexfecHeaderWriter2Test, ExpandsShortUlpfecPacketMaskWithBit15Set) {}

TEST(FlexfecHeaderWriter2Test,
     ContractsLongUlpfecPacketMaskWithBit46ClearBit47Clear) {}

TEST(FlexfecHeaderWriter2Test,
     ExpandsLongUlpfecPacketMaskWithBit46SetBit47Clear) {}

TEST(FlexfecHeaderWriter2Test,
     ExpandsLongUlpfecPacketMaskWithBit46ClearBit47Set) {}

TEST(FlexfecHeaderWriter2Test,
     ExpandsLongUlpfecPacketMaskWithBit46SetBit47Set) {}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadSmallUlpfecPacketHeaderWithMaskBit15Clear) {}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadSmallUlpfecPacketHeaderWithMaskBit15Set) {}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBits46And47Clear) {}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBit46SetBit47Clear) {}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadLargeUlpfecPacketHeaderMaskWithBit46ClearBit47Set) {}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBits46And47Set) {}

}  // namespace webrtc
