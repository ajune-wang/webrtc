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
constexpr uint8_t kMask0[] = {0xAB, 0xCD};              // First K bit is set.
constexpr uint8_t kMask1[] = {0x12, 0x34,               // First K bit cleared.
                              0xF6, 0x78, 0x9A, 0xBC};  // Second K bit set.
constexpr uint8_t kMask2[] = {0x12, 0x34,               //  First K bit cleared.
                              0x56, 0x78, 0x9A, 0xBC,   // Second K bit cleared.
                              0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};

// Reader tests.
constexpr uint8_t kFlexible = 0b00 << 6;
constexpr uint8_t kPtRecovery = 123;
constexpr uint8_t kLengthRecovery[] = {0xab, 0xcd};
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

void VerifyReadHeaders(size_t expected_fec_header_size,
                       const ReceivedFecPacket& read_packet,
                       std::vector<FecPacketStreamProperties> expected) {
  EXPECT_EQ(read_packet.fec_header_size, expected_fec_header_size);
  const size_t protected_streams_num = read_packet.protected_streams.size();
  EXPECT_EQ(protected_streams_num, expected.size());
  for (size_t i = 0; i < protected_streams_num; ++i) {
    SCOPED_TRACE(i);
    ProtectedStream protected_stream = read_packet.protected_streams[i];
    EXPECT_EQ(protected_stream.ssrc, expected[i].stream.ssrc);
    EXPECT_EQ(protected_stream.seq_num_base, expected[i].stream.seq_num_base);
    EXPECT_EQ(protected_stream.packet_mask_offset,
              expected[i].stream.packet_mask_offset);
    EXPECT_EQ(protected_stream.packet_mask_size,
              expected[i].stream.packet_mask_size);
    // Ensure that the K-bits are removed and the packet mask has been packed.
    EXPECT_THAT(rtc::MakeArrayView(read_packet.pkt->data.cdata() +
                                       protected_stream.packet_mask_offset,
                                   protected_stream.packet_mask_size),
                ElementsAreArray(expected[i].mask));
  }
  EXPECT_EQ(read_packet.pkt->data.size() - expected_fec_header_size,
            read_packet.protection_length);
}

}  // namespace

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit0SetSingleStream) {
  constexpr uint8_t kKBit0 = 1 << 7;
  constexpr size_t kExpectedFecHeaderSize = 12;
  constexpr uint16_t kSnBase = 0x0102;
  constexpr uint8_t kFlexfecPktMasks[] = {kKBit0 | 0x08, 0x81};
  constexpr uint8_t kUlpfecPacketMasks[] = {0x11, 0x02};
  constexpr uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0],  kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],      kTsRecovery[3],
      kSnBase >> 8,   kSnBase & 0xFF, kFlexfecPktMasks[0], kFlexfecPktMasks[1],
      kPayloadBits,   kPayloadBits,   kPayloadBits,        kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase,
                  .packet_mask_offset = 10,
                  .packet_mask_size = kFlexfecPacketMaskSizes[0]},
       .mask =
           rtc::MakeArrayView(kUlpfecPacketMasks, kFlexfecPacketMaskSizes[0])}};

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit1SetSingleStream) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 1 << 7;
  constexpr size_t kExpectedFecHeaderSize = 16;
  constexpr uint16_t kSnBase = 0x0102;
  constexpr uint8_t kFlexfecPktMasks[] = {kKBit0 | 0x48, 0x81,  //
                                          kKBit1 | 0x02, 0x11, 0x00, 0x21};
  constexpr uint8_t kUlpfecPacketMasks[] = {0x91, 0x02,  //
                                            0x08, 0x44, 0x00, 0x84};
  constexpr uint8_t kPacketData[] = {
      kFlexible,           kPtRecovery,         kLengthRecovery[0],
      kLengthRecovery[1],  kTsRecovery[0],      kTsRecovery[1],
      kTsRecovery[2],      kTsRecovery[3],      kSnBase >> 8,
      kSnBase & 0xFF,      kFlexfecPktMasks[0], kFlexfecPktMasks[1],
      kFlexfecPktMasks[2], kFlexfecPktMasks[3], kFlexfecPktMasks[4],
      kFlexfecPktMasks[5], kPayloadBits,        kPayloadBits,
      kPayloadBits,        kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase,
                  .packet_mask_offset = 10,
                  .packet_mask_size = kFlexfecPacketMaskSizes[1]},
       .mask =
           rtc::MakeArrayView(kUlpfecPacketMasks, kFlexfecPacketMaskSizes[1])}};

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithNoKBitsSetSingleStream) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 0 << 7;
  constexpr size_t kExpectedFecHeaderSize = 24;
  constexpr uint16_t kSnBase = 0x0102;
  constexpr uint8_t kFlexfecPacketMask[] = {kKBit0 | 0x48, 0x81,              //
                                            kKBit1 | 0x02, 0x11, 0x00, 0x21,  //
                                            0x01,          0x11, 0x11, 0x11,
                                            0x11,          0x11, 0x11, 0x11};
  constexpr uint8_t kUlpfecPacketMasks[] = {0x91, 0x02,              //
                                            0x08, 0x44, 0x00, 0x84,  //
                                            0x04, 0x44, 0x44, 0x44,
                                            0x44, 0x44, 0x44, 0x44};
  constexpr uint8_t kPacketData[] = {kFlexible,
                                     kPtRecovery,
                                     kLengthRecovery[0],
                                     kLengthRecovery[1],
                                     kTsRecovery[0],
                                     kTsRecovery[1],
                                     kTsRecovery[2],
                                     kTsRecovery[3],
                                     kSnBase >> 8,
                                     kSnBase & 0xFF,
                                     kFlexfecPacketMask[0],
                                     kFlexfecPacketMask[1],
                                     kFlexfecPacketMask[2],
                                     kFlexfecPacketMask[3],
                                     kFlexfecPacketMask[4],
                                     kFlexfecPacketMask[5],
                                     kFlexfecPacketMask[6],
                                     kFlexfecPacketMask[7],
                                     kFlexfecPacketMask[8],
                                     kFlexfecPacketMask[9],
                                     kFlexfecPacketMask[10],
                                     kFlexfecPacketMask[11],
                                     kFlexfecPacketMask[12],
                                     kFlexfecPacketMask[13],
                                     kPayloadBits,
                                     kPayloadBits,
                                     kPayloadBits,
                                     kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase,
                  .packet_mask_offset = 10,
                  .packet_mask_size = kFlexfecPacketMaskSizes[2]},
       .mask =
           rtc::MakeArrayView(kUlpfecPacketMasks, kFlexfecPacketMaskSizes[2])}};

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit0Set2Streams) {
  constexpr uint8_t kKBit0 = 1 << 7;
  constexpr size_t kExpectedFecHeaderSize = 16;
  constexpr uint16_t kSnBase0 = 0x0102;
  constexpr uint16_t kSnBase1 = 0x0304;
  constexpr uint8_t kFlexfecPktMask1[] = {kKBit0 | 0x08, 0x81};
  constexpr uint8_t kUlpfecPacketMask1[] = {0x11, 0x02};
  constexpr uint8_t kFlexfecPktMask2[] = {kKBit0 | 0x04, 0x41};
  constexpr uint8_t kUlpfecPacketMask2[] = {0x08, 0x82};

  constexpr uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,     kLengthRecovery[0],  kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1],  kTsRecovery[2],      kTsRecovery[3],
      kSnBase0 >> 8,  kSnBase0 & 0xFF, kFlexfecPktMask1[0], kFlexfecPktMask1[1],
      kSnBase1 >> 8,  kSnBase1 & 0xFF, kFlexfecPktMask2[0], kFlexfecPktMask2[1],
      kPayloadBits,   kPayloadBits,    kPayloadBits,        kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}, {.ssrc = 0x02}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase0,
                  .packet_mask_offset = 10,
                  .packet_mask_size = kFlexfecPacketMaskSizes[0]},
       .mask =
           rtc::MakeArrayView(kUlpfecPacketMask1, kFlexfecPacketMaskSizes[0])},
      {.stream = {.ssrc = 0x02,
                  .seq_num_base = kSnBase1,
                  .packet_mask_offset = 14,
                  .packet_mask_size = kFlexfecPacketMaskSizes[0]},
       .mask =
           rtc::MakeArrayView(kUlpfecPacketMask2, kFlexfecPacketMaskSizes[0])},
  };

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit1Set2Streams) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 1 << 7;
  constexpr size_t kExpectedFecHeaderSize = 24;
  constexpr uint16_t kSnBase0 = 0x0102;
  constexpr uint16_t kSnBase1 = 0x0304;
  constexpr uint8_t kFlexfecPktMask1[] = {kKBit0 | 0x48, 0x81,  //
                                          kKBit1 | 0x02, 0x11, 0x00, 0x21};
  constexpr uint8_t kUlpfecPacketMask1[] = {0x91, 0x02,  //
                                            0x08, 0x44, 0x00, 0x84};
  constexpr uint8_t kFlexfecPktMask2[] = {kKBit0 | 0x57, 0x82,  //
                                          kKBit1 | 0x04, 0x33, 0x00, 0x51};
  constexpr uint8_t kUlpfecPacketMask2[] = {0xAF, 0x04,  //
                                            0x10, 0xCC, 0x01, 0x44};
  constexpr uint8_t kPacketData[] = {
      kFlexible,           kPtRecovery,         kLengthRecovery[0],
      kLengthRecovery[1],  kTsRecovery[0],      kTsRecovery[1],
      kTsRecovery[2],      kTsRecovery[3],      kSnBase0 >> 8,
      kSnBase0 & 0xFF,     kFlexfecPktMask1[0], kFlexfecPktMask1[1],
      kFlexfecPktMask1[2], kFlexfecPktMask1[3], kFlexfecPktMask1[4],
      kFlexfecPktMask1[5], kSnBase1 >> 8,       kSnBase1 & 0xFF,
      kFlexfecPktMask2[0], kFlexfecPktMask2[1], kFlexfecPktMask2[2],
      kFlexfecPktMask2[3], kFlexfecPktMask2[4], kFlexfecPktMask2[5],
      kPayloadBits,        kPayloadBits,        kPayloadBits,
      kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}, {.ssrc = 0x02}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase0,
                  .packet_mask_offset = 10,
                  .packet_mask_size = kFlexfecPacketMaskSizes[1]},
       .mask =
           rtc::MakeArrayView(kUlpfecPacketMask1, kFlexfecPacketMaskSizes[1])},
      {.stream = {.ssrc = 0x02,
                  .seq_num_base = kSnBase1,
                  .packet_mask_offset = 18,
                  .packet_mask_size = kFlexfecPacketMaskSizes[1]},
       .mask =
           rtc::MakeArrayView(kUlpfecPacketMask2, kFlexfecPacketMaskSizes[1])},
  };

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithNoKBitsSet2Streams) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 0 << 7;
  constexpr size_t kExpectedFecHeaderSize = 40;
  constexpr uint16_t kSnBase0 = 0x0102;
  constexpr uint16_t kSnBase1 = 0x0304;
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

  constexpr uint8_t kPacketData[] = {kFlexible,
                                     kPtRecovery,
                                     kLengthRecovery[0],
                                     kLengthRecovery[1],
                                     kTsRecovery[0],
                                     kTsRecovery[1],
                                     kTsRecovery[2],
                                     kTsRecovery[3],
                                     kSnBase0 >> 8,
                                     kSnBase0 & 0xFF,
                                     kFlexfecPktMask1[0],
                                     kFlexfecPktMask1[1],
                                     kFlexfecPktMask1[2],
                                     kFlexfecPktMask1[3],
                                     kFlexfecPktMask1[4],
                                     kFlexfecPktMask1[5],
                                     kFlexfecPktMask1[6],
                                     kFlexfecPktMask1[7],
                                     kFlexfecPktMask1[8],
                                     kFlexfecPktMask1[9],
                                     kFlexfecPktMask1[10],
                                     kFlexfecPktMask1[11],
                                     kFlexfecPktMask1[12],
                                     kFlexfecPktMask1[13],
                                     kSnBase1 >> 8,
                                     kSnBase1 & 0xFF,
                                     kFlexfecPktMask2[0],
                                     kFlexfecPktMask2[1],
                                     kFlexfecPktMask2[2],
                                     kFlexfecPktMask2[3],
                                     kFlexfecPktMask2[4],
                                     kFlexfecPktMask2[5],
                                     kFlexfecPktMask2[6],
                                     kFlexfecPktMask2[7],
                                     kFlexfecPktMask2[8],
                                     kFlexfecPktMask2[9],
                                     kFlexfecPktMask2[10],
                                     kFlexfecPktMask2[11],
                                     kFlexfecPktMask2[12],
                                     kFlexfecPktMask2[13],
                                     kPayloadBits,
                                     kPayloadBits,
                                     kPayloadBits,
                                     kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}, {.ssrc = 0x02}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase0,
                  .packet_mask_offset = 10,
                  .packet_mask_size = kFlexfecPacketMaskSizes[2]},
       .mask =
           rtc::MakeArrayView(kUlpfecPacketMask1, kFlexfecPacketMaskSizes[2])},
      {.stream = {.ssrc = 0x02,
                  .seq_num_base = kSnBase1,
                  .packet_mask_offset = 26,
                  .packet_mask_size = kFlexfecPacketMaskSizes[2]},
       .mask =
           rtc::MakeArrayView(kUlpfecPacketMask2, kFlexfecPacketMaskSizes[2])},
  };

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithMultipleStreamsMultipleMasks) {
  constexpr uint8_t kBit0 = 0 << 7;
  constexpr uint8_t kBit1 = 1 << 7;
  constexpr size_t kExpectedFecHeaderSize = 44;
  constexpr uint16_t kSnBase0 = 0x0102;
  constexpr uint16_t kSnBase1 = 0x0304;
  constexpr uint16_t kSnBase2 = 0x0506;
  constexpr uint16_t kSnBase3 = 0x0708;
  constexpr uint8_t kFlexfecPacketMasks[4][14] = {
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
  constexpr uint8_t kPacketData[] = {kFlexible,
                                     kPtRecovery,
                                     kLengthRecovery[0],
                                     kLengthRecovery[1],
                                     kTsRecovery[0],
                                     kTsRecovery[1],
                                     kTsRecovery[2],
                                     kTsRecovery[3],
                                     kSnBase0 >> 8,
                                     kSnBase0 & 0xFF,
                                     kFlexfecPacketMasks[0][0],
                                     kFlexfecPacketMasks[0][1],
                                     kSnBase1 >> 8,
                                     kSnBase1 & 0xFF,
                                     kFlexfecPacketMasks[1][0],
                                     kFlexfecPacketMasks[1][1],
                                     kFlexfecPacketMasks[1][2],
                                     kFlexfecPacketMasks[1][3],
                                     kFlexfecPacketMasks[1][4],
                                     kFlexfecPacketMasks[1][5],
                                     kSnBase2 >> 8,
                                     kSnBase2 & 0xFF,
                                     kFlexfecPacketMasks[2][0],
                                     kFlexfecPacketMasks[2][1],
                                     kFlexfecPacketMasks[2][2],
                                     kFlexfecPacketMasks[2][3],
                                     kFlexfecPacketMasks[2][4],
                                     kFlexfecPacketMasks[2][5],
                                     kFlexfecPacketMasks[2][6],
                                     kFlexfecPacketMasks[2][7],
                                     kFlexfecPacketMasks[2][8],
                                     kFlexfecPacketMasks[2][9],
                                     kFlexfecPacketMasks[2][10],
                                     kFlexfecPacketMasks[2][11],
                                     kFlexfecPacketMasks[2][12],
                                     kFlexfecPacketMasks[2][13],
                                     kSnBase3 >> 8,
                                     kSnBase3 & 0xFF,
                                     kFlexfecPacketMasks[3][0],
                                     kFlexfecPacketMasks[3][1],
                                     kFlexfecPacketMasks[3][2],
                                     kFlexfecPacketMasks[3][3],
                                     kFlexfecPacketMasks[3][4],
                                     kFlexfecPacketMasks[3][5],
                                     kPayloadBits,
                                     kPayloadBits,
                                     kPayloadBits,
                                     kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {
      {.ssrc = 0x01}, {.ssrc = 0x02}, {.ssrc = 0x03}, {.ssrc = 0x04}};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase0,
                  .packet_mask_offset = 10,
                  .packet_mask_size = kFlexfecPacketMaskSizes[0]},
       .mask = rtc::MakeArrayView(kUlpfecPacketMasks[0],
                                  kFlexfecPacketMaskSizes[0])},
      {.stream = {.ssrc = 0x02,
                  .seq_num_base = kSnBase1,
                  .packet_mask_offset = 14,
                  .packet_mask_size = kFlexfecPacketMaskSizes[1]},
       .mask = rtc::MakeArrayView(kUlpfecPacketMasks[1],
                                  kFlexfecPacketMaskSizes[1])},
      {.stream = {.ssrc = 0x03,
                  .seq_num_base = kSnBase2,
                  .packet_mask_offset = 22,
                  .packet_mask_size = kFlexfecPacketMaskSizes[2]},
       .mask = rtc::MakeArrayView(kUlpfecPacketMasks[2],
                                  kFlexfecPacketMaskSizes[2])},
      {.stream = {.ssrc = 0x04,
                  .seq_num_base = kSnBase3,
                  .packet_mask_offset = 38,
                  .packet_mask_size = kFlexfecPacketMaskSizes[1]},
       .mask = rtc::MakeArrayView(kUlpfecPacketMasks[3],
                                  kFlexfecPacketMaskSizes[1])},
  };

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReader2Test, ReadPacketWithoutProtectedSsrcsShouldFail) {
  constexpr uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  // No protected ssrcs.
  read_packet.protected_streams = {};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReader2Test,
     ReadPacketWithoutStreamSpecificHeaderShouldFail) {
  // Simulate short received packet.
  constexpr uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReader2Test, ReadShortPacketWithKBit0SetShouldFail) {
  // Simulate short received packet.
  uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3],
      kSnBases[0][0], kSnBases[0][1], kMask0[0],          kMask0[1]};
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
  uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3],
      kSnBases[0][0], kSnBases[0][1], kMask1[0],          kMask1[1],
      kMask1[2],      kMask1[3],      kMask1[4],          kMask1[5]};
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
  uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3],
      kSnBases[0][0], kSnBases[0][1], kMask2[0],          kMask2[1],
      kMask2[2],      kMask2[3],      kMask2[4],          kMask2[5],
      kMask2[6],      kMask2[7],      kMask2[8],          kMask2[9],
      kMask2[10],     kMask2[11],     kMask2[12],         kMask2[13]};
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
  uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3],
      kSnBases[0][0], kSnBases[0][1], kMask0[0],          kMask0[1],
      kSnBases[1][0], kSnBases[1][1], kMask2[0],          kMask2[1],
      kMask2[2],      kMask2[3],      kMask2[4],          kMask2[5],
      kMask2[6],      kMask2[7],      kMask2[8],          kMask2[9],
      kMask2[10],     kMask2[11],     kMask2[12],         kMask2[13]};
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
