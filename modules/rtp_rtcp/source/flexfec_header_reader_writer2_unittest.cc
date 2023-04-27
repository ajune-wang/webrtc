/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
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

#include "api/scoped_refptr.h"
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
using ReceivedFecPacket = ForwardErrorCorrection::ReceivedFecPacket;

/*
 * Commented out as for now the Reader and Writer are incompatible.
 * Uncomment and use after Writer is updated.

// General. Assume single-stream protection.
constexpr uint32_t kMediaSsrc = 1254983;
constexpr uint16_t kMediaStartSeqNum = 825;
constexpr size_t kMediaPacketLength = 1234;
constexpr uint32_t kFlexfecSsrc = 52142;

constexpr size_t kFlexfecHeaderSizes[] = {20, 24, 32};
constexpr size_t kFlexfecPacketMaskOffset = 18;

*/

constexpr size_t kFlexfecPacketMaskSizes[] = {2, 6, 14};
constexpr size_t kFlexfecMaxPacketSize = kFlexfecPacketMaskSizes[2];

// Reader tests.
constexpr uint8_t kNoRBit = 0 << 7;
constexpr uint8_t kNoFBit = 0 << 6;
constexpr uint8_t kPtRecovery = 123;
constexpr uint8_t kLengthRecov[] = {0xab, 0xcd};
constexpr uint8_t kTsRecovery[] = {0x01, 0x23, 0x45, 0x67};
constexpr size_t kMaxSsrcNum = 4;
constexpr uint8_t kSnBases[4][2] = {{0x01, 0x01},
                                    {0x02, 0x02},
                                    {0x03, 0x03},
                                    {0x04, 0x04}};
constexpr uint8_t kPayloadBits = 0x00;

std::unique_ptr<uint8_t[]> GeneratePacketMask(size_t packet_mask_size,
                                              uint64_t seed) {
  Random random(seed);
  std::unique_ptr<uint8_t[]> packet_mask(new uint8_t[kFlexfecMaxPacketSize]);
  memset(packet_mask.get(), 0, kFlexfecMaxPacketSize);
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

/*
 * Commented out as for now the Reader and Writer are incompatible.
 * Uncomment and use after Writer is updated.

rtc::scoped_refptr<Packet> WriteHeader(const uint8_t* packet_mask,
                                       size_t packet_mask_size) {
  FlexfecHeaderWriter2 writer;
  rtc::scoped_refptr<Packet> written_packet(new Packet());
  written_packet->data.SetSize(kMediaPacketLength);
  uint8_t* data = written_packet->data.MutableData();
  for (size_t i = 0; i < written_packet->data.size(); ++i) {
    data[i] = i;  // Actual content doesn't matter.
  }
  writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, packet_mask,
                           packet_mask_size, written_packet.get());
  return written_packet;
}

std::unique_ptr<ReceivedFecPacket> ReadHeader(const Packet& written_packet) {
  FlexfecHeaderReader2 reader;
  std::unique_ptr<ReceivedFecPacket> read_packet(new ReceivedFecPacket());
  read_packet->ssrc = kFlexfecSsrc;
  read_packet->pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet->pkt->data = written_packet.data;
  EXPECT_TRUE(reader.ReadFecHeader(read_packet.get()));
  return read_packet;
}

*/

void VerifyReadHeaders(
    size_t expected_fec_header_size,
    const size_t* expected_mask_offsets,
    const uint8_t expected_packet_masks[kMaxSsrcNum][kFlexfecMaxPacketSize],
    const size_t* expected_packet_mask_sizes,
    const ReceivedFecPacket& read_packet) {
  const size_t protected_ssrcs_num = read_packet.protected_ssrcs.size();
  EXPECT_EQ(expected_fec_header_size, read_packet.fec_header_size);
  EXPECT_EQ(read_packet.seq_num_bases.size(), protected_ssrcs_num);
  for (size_t i = 0; i < protected_ssrcs_num; ++i) {
    EXPECT_EQ(read_packet.seq_num_bases[i],
              ByteReader<uint16_t>::ReadBigEndian(kSnBases[i]));
  }
  EXPECT_THAT(::testing::make_tuple(expected_mask_offsets, protected_ssrcs_num),
              ::testing::ElementsAreArray(read_packet.packet_mask_offsets));
  EXPECT_THAT(
      ::testing::make_tuple(expected_packet_mask_sizes, protected_ssrcs_num),
      ::testing::ElementsAreArray(read_packet.packet_mask_sizes));
  EXPECT_EQ(read_packet.pkt->data.size() - expected_fec_header_size,
            read_packet.protection_length);
  // Ensure that the K-bits are removed and the packet mask has been packed.
  for (size_t i = 0; i < protected_ssrcs_num; ++i) {
    EXPECT_THAT(::testing::make_tuple(read_packet.pkt->data.cdata() +
                                          read_packet.packet_mask_offsets[i],
                                      read_packet.packet_mask_sizes[i]),
                ::testing::ElementsAreArray(expected_packet_masks[i],
                                            expected_packet_mask_sizes[i]));
  }
}

/*
 * Commented out as for now the Reader and Writer are incompatible.
 * Uncomment and use after Writer is updated.

void VerifyFinalizedHeaders(const uint8_t* expected_packet_mask,
                            size_t expected_packet_mask_size,
                            const Packet& written_packet) {
  const uint8_t* packet = written_packet.data.cdata();
  EXPECT_EQ(0x00, packet[0] & 0x80);  // F bit clear.
  EXPECT_EQ(0x00, packet[0] & 0x40);  // R bit clear.
  EXPECT_EQ(0x01, packet[8]);         // SSRCCount = 1.
  EXPECT_EQ(kMediaSsrc, ByteReader<uint32_t>::ReadBigEndian(packet + 12));
  EXPECT_EQ(kMediaStartSeqNum,
            ByteReader<uint16_t>::ReadBigEndian(packet + 16));
  EXPECT_THAT(::testing::make_tuple(packet + kFlexfecPacketMaskOffset,
                                    expected_packet_mask_size),
              ::testing::ElementsAreArray(expected_packet_mask,
                                          expected_packet_mask_size));
}

void VerifyWrittenAndReadHeaders(size_t expected_fec_header_size,
                                 const uint8_t* expected_packet_mask,
                                 size_t expected_packet_mask_size,
                                 const Packet& written_packet,
                                 const ReceivedFecPacket& read_packet) {
  EXPECT_EQ(kFlexfecSsrc, read_packet.ssrc);
  EXPECT_EQ(expected_fec_header_size, read_packet.fec_header_size);
  EXPECT_THAT(read_packet.protected_ssrcs, ::testing::ElementsAre(kMediaSsrc));
  EXPECT_THAT(read_packet.seq_num_bases,
              ::testing::ElementsAre(kMediaStartSeqNum));
  EXPECT_THAT(read_packet.packet_mask_offsets,
              ::testing::ElementsAre(kFlexfecPacketMaskOffset));
  ASSERT_THAT(read_packet.packet_mask_sizes,
              ::testing::ElementsAre(expected_packet_mask_size));
  EXPECT_EQ(written_packet.data.size() - expected_fec_header_size,
            read_packet.protection_length);
  // Verify that the call to ReadFecHeader did normalize the packet masks.
  EXPECT_THAT(::testing::make_tuple(
                  read_packet.pkt->data.cdata() + kFlexfecPacketMaskOffset,
                  read_packet.packet_mask_sizes[0]),
              ::testing::ElementsAreArray(expected_packet_mask,
                                          expected_packet_mask_size));
  // Verify that the call to ReadFecHeader did not tamper with the payload.
  EXPECT_THAT(::testing::make_tuple(
                  read_packet.pkt->data.cdata() + read_packet.fec_header_size,
                  read_packet.pkt->data.size() - read_packet.fec_header_size),
              ::testing::ElementsAreArray(
                  written_packet.data.cdata() + expected_fec_header_size,
                  written_packet.data.size() - expected_fec_header_size));
}

*/

}  // namespace

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit0SetSingleStream) {
  constexpr uint8_t kKBit0 = 1 << 7;
  constexpr size_t kExpectedPacketMaskSizes[] = {kFlexfecPacketMaskSizes[0]};
  constexpr size_t kExpectedFecHeaderSize = 12;
  constexpr uint8_t kFlexfecPktMasks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {kKBit0 | 0x08, 0x81}};
  constexpr uint8_t kUlpfecPacketMasks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {0x11, 0x02}};
  constexpr size_t kExpectedMaskOffsets[] = {10};
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit,      kPtRecovery,
      kLengthRecov[0],        kLengthRecov[1],
      kTsRecovery[0],         kTsRecovery[1],
      kTsRecovery[2],         kTsRecovery[3],
      kSnBases[0][0],         kSnBases[0][1],
      kFlexfecPktMasks[0][0], kFlexfecPktMasks[0][1],
      kPayloadBits,           kPayloadBits,
      kPayloadBits,           kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_ssrcs = {0x01};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  VerifyReadHeaders(kExpectedFecHeaderSize, kExpectedMaskOffsets,
                    kUlpfecPacketMasks, kExpectedPacketMaskSizes, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit1SetSingleStream) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 1 << 7;
  constexpr size_t kExpectedPacketMaskSizes[] = {kFlexfecPacketMaskSizes[1]};
  constexpr size_t kExpectedFecHeaderSize = 16;
  constexpr uint8_t kFlexfecPktMasks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {kKBit0 | 0x48, 0x81, kKBit1 | 0x02, 0x11, 0x00, 0x21}};
  constexpr uint8_t kUlpfecPacketMasks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {0x91, 0x02, 0x08, 0x44, 0x00, 0x84}};
  constexpr size_t kExpectedMaskOffsets[] = {10};
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit,      kPtRecovery,
      kLengthRecov[0],        kLengthRecov[1],
      kTsRecovery[0],         kTsRecovery[1],
      kTsRecovery[2],         kTsRecovery[3],
      kSnBases[0][0],         kSnBases[0][1],
      kFlexfecPktMasks[0][0], kFlexfecPktMasks[0][1],
      kFlexfecPktMasks[0][2], kFlexfecPktMasks[0][3],
      kFlexfecPktMasks[0][4], kFlexfecPktMasks[0][5],
      kPayloadBits,           kPayloadBits,
      kPayloadBits,           kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_ssrcs = {0x01};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  VerifyReadHeaders(kExpectedFecHeaderSize, kExpectedMaskOffsets,
                    kUlpfecPacketMasks, kExpectedPacketMaskSizes, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithNoKBitsSetSingleStream) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 0 << 7;
  constexpr size_t kExpectedPacketMaskSizes[] = {kFlexfecPacketMaskSizes[2]};
  constexpr size_t kExpectedFecHeaderSize = 24;
  constexpr uint8_t kFlxfcPktMsks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {kKBit0 | 0x48, 0x81, kKBit1 | 0x02, 0x11, 0x00, 0x21, 0x01, 0x11, 0x11,
       0x11, 0x11, 0x11, 0x11, 0x11}};
  constexpr uint8_t kUlpfecPacketMasks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {0x91, 0x02, 0x08, 0x44, 0x00, 0x84, 0x04, 0x44, 0x44, 0x44, 0x44, 0x44,
       0x44, 0x44}};
  constexpr size_t kExpectedMaskOffsets[] = {10};
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit,    kPtRecovery,          kLengthRecov[0],
      kLengthRecov[1],      kTsRecovery[0],       kTsRecovery[1],
      kTsRecovery[2],       kTsRecovery[3],       kSnBases[0][0],
      kSnBases[0][1],       kFlxfcPktMsks[0][0],  kFlxfcPktMsks[0][1],
      kFlxfcPktMsks[0][2],  kFlxfcPktMsks[0][3],  kFlxfcPktMsks[0][4],
      kFlxfcPktMsks[0][5],  kFlxfcPktMsks[0][6],  kFlxfcPktMsks[0][7],
      kFlxfcPktMsks[0][8],  kFlxfcPktMsks[0][9],  kFlxfcPktMsks[0][10],
      kFlxfcPktMsks[0][11], kFlxfcPktMsks[0][12], kFlxfcPktMsks[0][13],
      kPayloadBits,         kPayloadBits,         kPayloadBits,
      kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_ssrcs = {0x01};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  VerifyReadHeaders(kExpectedFecHeaderSize, kExpectedMaskOffsets,
                    kUlpfecPacketMasks, kExpectedPacketMaskSizes, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit0Set2Streams) {
  constexpr uint8_t kKBit0 = 1 << 7;
  constexpr size_t kExpectedPacketMaskSizes[] = {kFlexfecPacketMaskSizes[0],
                                                 kFlexfecPacketMaskSizes[0]};
  constexpr size_t kExpectedFecHeaderSize = 16;
  constexpr uint8_t kFlexfecPktMasks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {kKBit0 | 0x08, 0x81}, {kKBit0 | 0x04, 0x41}};
  constexpr uint8_t kUlpfecPacketMasks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {0x11, 0x02}, {0x08, 0x82}};
  constexpr size_t kExpectedMaskOffsets[] = {10, 14};
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit,      kPtRecovery,
      kLengthRecov[0],        kLengthRecov[1],
      kTsRecovery[0],         kTsRecovery[1],
      kTsRecovery[2],         kTsRecovery[3],
      kSnBases[0][0],         kSnBases[0][1],
      kFlexfecPktMasks[0][0], kFlexfecPktMasks[0][1],
      kSnBases[1][0],         kSnBases[1][1],
      kFlexfecPktMasks[1][0], kFlexfecPktMasks[1][1],
      kPayloadBits,           kPayloadBits,
      kPayloadBits,           kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_ssrcs = {0x01, 0x02};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  VerifyReadHeaders(kExpectedFecHeaderSize, kExpectedMaskOffsets,
                    kUlpfecPacketMasks, kExpectedPacketMaskSizes, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit1Set2Streams) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 1 << 7;
  constexpr size_t kExpectedPacketMaskSizes[] = {kFlexfecPacketMaskSizes[1],
                                                 kFlexfecPacketMaskSizes[1]};
  constexpr size_t kExpectedFecHeaderSize = 24;
  constexpr uint8_t kFlexfecPktMasks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {kKBit0 | 0x48, 0x81, kKBit1 | 0x02, 0x11, 0x00, 0x21},
      {kKBit0 | 0x57, 0x82, kKBit1 | 0x04, 0x33, 0x00, 0x51}};
  constexpr uint8_t kUlpfecPacketMasks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {0x91, 0x02, 0x08, 0x44, 0x00, 0x84},
      {0xAF, 0x04, 0x10, 0xCC, 0x01, 0x44}};
  constexpr size_t kExpectedMaskOffsets[] = {10, 18};
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit,      kPtRecovery,
      kLengthRecov[0],        kLengthRecov[1],
      kTsRecovery[0],         kTsRecovery[1],
      kTsRecovery[2],         kTsRecovery[3],
      kSnBases[0][0],         kSnBases[0][1],
      kFlexfecPktMasks[0][0], kFlexfecPktMasks[0][1],
      kFlexfecPktMasks[0][2], kFlexfecPktMasks[0][3],
      kFlexfecPktMasks[0][4], kFlexfecPktMasks[0][5],
      kSnBases[1][0],         kSnBases[1][1],
      kFlexfecPktMasks[1][0], kFlexfecPktMasks[1][1],
      kFlexfecPktMasks[1][2], kFlexfecPktMasks[1][3],
      kFlexfecPktMasks[1][4], kFlexfecPktMasks[1][5],
      kPayloadBits,           kPayloadBits,
      kPayloadBits,           kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_ssrcs = {0x01, 0x02};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  VerifyReadHeaders(kExpectedFecHeaderSize, kExpectedMaskOffsets,
                    kUlpfecPacketMasks, kExpectedPacketMaskSizes, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithNoKBitsSet2Streams) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 0 << 7;
  constexpr size_t kExpectedPacketMaskSizes[] = {kFlexfecPacketMaskSizes[2],
                                                 kFlexfecPacketMaskSizes[2]};
  constexpr size_t kExpectedFecHeaderSize = 40;
  constexpr uint8_t kFlxfcPktMsks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {kKBit0 | 0x48, 0x81, kKBit1 | 0x02, 0x11, 0x00, 0x21, 0x01, 0x11, 0x11,
       0x11, 0x11, 0x11, 0x11, 0x11},
      {kKBit0 | 0x32, 0x84, kKBit1 | 0x05, 0x23, 0x00, 0x55, 0xA3, 0x22, 0x22,
       0x22, 0x22, 0x22, 0x22, 0x35}};
  constexpr uint8_t kUlpfecPacketMasks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {0x91, 0x02, 0x08, 0x44, 0x00, 0x84, 0x04, 0x44, 0x44, 0x44, 0x44, 0x44,
       0x44, 0x44},
      {0x65, 0x08, 0x14, 0x8C, 0x01, 0x56, 0x8C, 0x88, 0x88, 0x88, 0x88, 0x88,
       0x88, 0xD4}};
  constexpr size_t kExpectedMaskOffsets[] = {10, 26};
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit,    kPtRecovery,          kLengthRecov[0],
      kLengthRecov[1],      kTsRecovery[0],       kTsRecovery[1],
      kTsRecovery[2],       kTsRecovery[3],       kSnBases[0][0],
      kSnBases[0][1],       kFlxfcPktMsks[0][0],  kFlxfcPktMsks[0][1],
      kFlxfcPktMsks[0][2],  kFlxfcPktMsks[0][3],  kFlxfcPktMsks[0][4],
      kFlxfcPktMsks[0][5],  kFlxfcPktMsks[0][6],  kFlxfcPktMsks[0][7],
      kFlxfcPktMsks[0][8],  kFlxfcPktMsks[0][9],  kFlxfcPktMsks[0][10],
      kFlxfcPktMsks[0][11], kFlxfcPktMsks[0][12], kFlxfcPktMsks[0][13],
      kSnBases[1][0],       kSnBases[1][1],       kFlxfcPktMsks[1][0],
      kFlxfcPktMsks[1][1],  kFlxfcPktMsks[1][2],  kFlxfcPktMsks[1][3],
      kFlxfcPktMsks[1][4],  kFlxfcPktMsks[1][5],  kFlxfcPktMsks[1][6],
      kFlxfcPktMsks[1][7],  kFlxfcPktMsks[1][8],  kFlxfcPktMsks[1][9],
      kFlxfcPktMsks[1][10], kFlxfcPktMsks[1][11], kFlxfcPktMsks[1][12],
      kFlxfcPktMsks[1][13], kPayloadBits,         kPayloadBits,
      kPayloadBits,         kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_ssrcs = {0x01, 0x02};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  VerifyReadHeaders(kExpectedFecHeaderSize, kExpectedMaskOffsets,
                    kUlpfecPacketMasks, kExpectedPacketMaskSizes, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithMultipleStreamsMultipleMasks) {
  constexpr uint8_t kBit0 = 0 << 7;
  constexpr uint8_t kBit1 = 1 << 7;
  constexpr size_t kExpectedPacketMaskSizes[] = {
      kFlexfecPacketMaskSizes[0], kFlexfecPacketMaskSizes[1],
      kFlexfecPacketMaskSizes[2], kFlexfecPacketMaskSizes[1]};
  constexpr size_t kExpectedFecHeaderSize = 44;
  constexpr uint8_t kFlxfcPktMsks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {kBit1 | 0x29, 0x91},
      {kBit0 | 0x32, 0xA1, kBit1 | 0x02, 0x11, 0x00, 0x21},
      {kBit0 | 0x48, 0x81, kBit0 | 0x02, 0x11, 0x00, 0x21, 0x01, 0x11, 0x11,
       0x11, 0x11, 0x11, 0x11, 0x11},
      {kBit0 | 0x32, 0x84, kBit1 | 0x05, 0x23, 0x00, 0x55}};
  constexpr uint8_t kUlpfecPacketMasks[kMaxSsrcNum][kFlexfecMaxPacketSize] = {
      {0x53, 0x22},
      {0x65, 0x42, 0x08, 0x44, 0x00, 0x84},
      {0x91, 0x02, 0x08, 0x44, 0x00, 0x84, 0x04, 0x44, 0x44, 0x44, 0x44, 0x44,
       0x44, 0x44},
      {0x65, 0x08, 0x14, 0x8C, 0x01, 0x54}};
  constexpr size_t kExpectedMaskOffsets[] = {10, 14, 22, 38};
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
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_ssrcs = {0x01, 0x02, 0x03, 0x04};

  FlexfecHeaderReader2 reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  VerifyReadHeaders(kExpectedFecHeaderSize, kExpectedMaskOffsets,
                    kUlpfecPacketMasks, kExpectedPacketMaskSizes, read_packet);
}

TEST(FlexfecHeaderReader2Test, ReadPacketWithoutProtectedSsrcsShouldFail) {
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit, kPtRecovery,    kLengthRecov[0], kLengthRecov[1],
      kTsRecovery[0],    kTsRecovery[1], kTsRecovery[2],  kTsRecovery[3]};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  // No protected ssrcs.
  read_packet.protected_ssrcs = {};

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
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(kPacketData, packet_length);
  read_packet.protected_ssrcs = {0x01};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReader2Test, ReadShortPacketWithKBit0SetShouldFail) {
  // Simulate short received packet.
  const size_t packet_mask_size = kFlexfecPacketMaskSizes[0];
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(0, packet_mask.get());  // Set kbit0.
  uint8_t kPacketData[] = {kNoRBit | kNoFBit, kPtRecovery,    kLengthRecov[0],
                           kLengthRecov[1],   kTsRecovery[0], kTsRecovery[1],
                           kTsRecovery[2],    kTsRecovery[3], kSnBases[0][0],
                           kSnBases[0][1],    packet_mask[0], packet_mask[1]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  // Expected to have 2 bytes of mask but length of packet misses 1 byte.
  read_packet.pkt->data.SetData(kPacketData, sizeof(kPacketData) - 1);
  read_packet.protected_ssrcs = {0x01};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReader2Test, ReadShortPacketWithKBit1SetShouldFail) {
  // Simulate short received packet.
  const size_t packet_mask_size = kFlexfecPacketMaskSizes[1];
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  // Clear kbit0 and set kbit1.
  ClearBit(0, packet_mask.get());
  SetBit(16, packet_mask.get());
  uint8_t kPacketData[] = {
      kNoRBit | kNoFBit, kPtRecovery,    kLengthRecov[0], kLengthRecov[1],
      kTsRecovery[0],    kTsRecovery[1], kTsRecovery[2],  kTsRecovery[3],
      kSnBases[0][0],    kSnBases[0][1], packet_mask[0],  packet_mask[1],
      packet_mask[2],    packet_mask[3], packet_mask[4],  packet_mask[5]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  // Expected to have 6 bytes of mask but length of packet misses 2 bytes.
  read_packet.pkt->data.SetData(kPacketData, sizeof(kPacketData) - 2);
  read_packet.protected_ssrcs = {0x01};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReader2Test, ReadShortPacketWithKBit1ClearedShouldFail) {
  // Simulate short received packet.
  const size_t packet_mask_size = kFlexfecPacketMaskSizes[2];
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
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
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  // Expected to have 14 bytes of mask but length of packet misses 2 bytes.
  read_packet.pkt->data.SetData(kPacketData, sizeof(kPacketData) - 2);
  read_packet.protected_ssrcs = {0x01};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReader2Test, ReadShortPacketMultipleStreamsShouldFail) {
  // Simulate short received packet with 2 protected ssrcs.
  auto packet_mask_1 = GeneratePacketMask(kFlexfecPacketMaskSizes[0], 0xabcd);
  auto packet_mask_2 = GeneratePacketMask(kFlexfecPacketMaskSizes[2], 0xabcd);
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
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  // Subtract 2 bytes from length, so the read will fail on parsing second mask.
  read_packet.pkt->data.SetData(kPacketData, sizeof(kPacketData) - 2);
  read_packet.protected_ssrcs = {0x01, 0x02};

  FlexfecHeaderReader2 reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

/*
 * Commented out as for now the Reader and Writer are incompatible.
 * Uncomment and fix after Writer is updated.

TEST(FlexfecHeaderWriter2Test, FinalizesHeaderWithKBit0Set) {
  constexpr size_t kExpectedPacketMaskSize = 2;
  constexpr uint8_t kFlexfecPacketMask[] = {0x88, 0x81};
  constexpr uint8_t kUlpfecPacketMask[] = {0x11, 0x02};
  Packet written_packet;
  written_packet.data.SetSize(kMediaPacketLength);
  uint8_t* data = written_packet.data.MutableData();
  for (size_t i = 0; i < written_packet.data.size(); ++i) {
    data[i] = i;
  }

  FlexfecHeaderWriter2 writer;
  writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, kUlpfecPacketMask,
                           sizeof(kUlpfecPacketMask), &written_packet);

  VerifyFinalizedHeaders(kFlexfecPacketMask, kExpectedPacketMaskSize,
                         written_packet);
}

TEST(FlexfecHeaderWriter2Test, FinalizesHeaderWithKBit1Set) {
  constexpr size_t kExpectedPacketMaskSize = 6;
  constexpr uint8_t kFlexfecPacketMask[] = {0x48, 0x81, 0x82, 0x11, 0x00, 0x21};
  constexpr uint8_t kUlpfecPacketMask[] = {0x91, 0x02, 0x08, 0x44, 0x00, 0x84};
  Packet written_packet;
  written_packet.data.SetSize(kMediaPacketLength);
  uint8_t* data = written_packet.data.MutableData();
  for (size_t i = 0; i < written_packet.data.size(); ++i) {
    data[i] = i;
  }

  FlexfecHeaderWriter2 writer;
  writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, kUlpfecPacketMask,
                           sizeof(kUlpfecPacketMask), &written_packet);

  VerifyFinalizedHeaders(kFlexfecPacketMask, kExpectedPacketMaskSize,
                         written_packet);
}

TEST(FlexfecHeaderWriter2Test, FinalizesHeaderWithKBit2Set) {
  constexpr size_t kExpectedPacketMaskSize = 14;
  constexpr uint8_t kFlexfecPacketMask[] = {
      0x11, 0x11,                                     // K-bit 0 clear.
      0x11, 0x11, 0x11, 0x10,                         // K-bit 1 clear.
      0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // K-bit 2 set.
  };
  constexpr uint8_t kUlpfecPacketMask[] = {0x22, 0x22, 0x44, 0x44, 0x44, 0x41};
  Packet written_packet;
  written_packet.data.SetSize(kMediaPacketLength);
  uint8_t* data = written_packet.data.MutableData();
  for (size_t i = 0; i < written_packet.data.size(); ++i) {
    data[i] = i;
  }

  FlexfecHeaderWriter2 writer;
  writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, kUlpfecPacketMask,
                           sizeof(kUlpfecPacketMask), &written_packet);

  VerifyFinalizedHeaders(kFlexfecPacketMask, kExpectedPacketMaskSize,
                         written_packet);
}

TEST(FlexfecHeaderWriter2Test, ContractsShortUlpfecPacketMaskWithBit15Clear) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  ClearBit(15, packet_mask.get());

  FlexfecHeaderWriter2 writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kFlexfecPacketMaskSizes[0], min_packet_mask_size);
  EXPECT_EQ(kFlexfecHeaderSizes[0], writer.FecHeaderSize(min_packet_mask_size));
}

TEST(FlexfecHeaderWriter2Test, ExpandsShortUlpfecPacketMaskWithBit15Set) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(15, packet_mask.get());

  FlexfecHeaderWriter2 writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kFlexfecPacketMaskSizes[1], min_packet_mask_size);
  EXPECT_EQ(kFlexfecHeaderSizes[1], writer.FecHeaderSize(min_packet_mask_size));
}

TEST(FlexfecHeaderWriter2Test,
     ContractsLongUlpfecPacketMaskWithBit46ClearBit47Clear) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  ClearBit(46, packet_mask.get());
  ClearBit(47, packet_mask.get());

  FlexfecHeaderWriter2 writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kFlexfecPacketMaskSizes[1], min_packet_mask_size);
  EXPECT_EQ(kFlexfecHeaderSizes[1], writer.FecHeaderSize(min_packet_mask_size));
}

TEST(FlexfecHeaderWriter2Test,
     ExpandsLongUlpfecPacketMaskWithBit46SetBit47Clear) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(46, packet_mask.get());
  ClearBit(47, packet_mask.get());

  FlexfecHeaderWriter2 writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kFlexfecPacketMaskSizes[2], min_packet_mask_size);
  EXPECT_EQ(kFlexfecHeaderSizes[2], writer.FecHeaderSize(min_packet_mask_size));
}

TEST(FlexfecHeaderWriter2Test,
     ExpandsLongUlpfecPacketMaskWithBit46ClearBit47Set) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  ClearBit(46, packet_mask.get());
  SetBit(47, packet_mask.get());

  FlexfecHeaderWriter2 writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kFlexfecPacketMaskSizes[2], min_packet_mask_size);
  EXPECT_EQ(kFlexfecHeaderSizes[2], writer.FecHeaderSize(min_packet_mask_size));
}

TEST(FlexfecHeaderWriter2Test,
     ExpandsLongUlpfecPacketMaskWithBit46SetBit47Set) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(46, packet_mask.get());
  SetBit(47, packet_mask.get());

  FlexfecHeaderWriter2 writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kFlexfecPacketMaskSizes[2], min_packet_mask_size);
  EXPECT_EQ(kFlexfecHeaderSizes[2], writer.FecHeaderSize(min_packet_mask_size));
}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadSmallUlpfecPacketHeaderWithMaskBit15Clear) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  ClearBit(15, packet_mask.get());

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyWrittenAndReadHeaders(kFlexfecHeaderSizes[0], packet_mask.get(),
                              kFlexfecPacketMaskSizes[0], *written_packet,
                              *read_packet);
}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadSmallUlpfecPacketHeaderWithMaskBit15Set) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(15, packet_mask.get());

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyWrittenAndReadHeaders(kFlexfecHeaderSizes[1], packet_mask.get(),
                              kFlexfecPacketMaskSizes[1], *written_packet,
                              *read_packet);
}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBits46And47Clear) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  ClearBit(46, packet_mask.get());
  ClearBit(47, packet_mask.get());

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyWrittenAndReadHeaders(kFlexfecHeaderSizes[1], packet_mask.get(),
                              kFlexfecPacketMaskSizes[1], *written_packet,
                              *read_packet);
}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBit46SetBit47Clear) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(46, packet_mask.get());
  ClearBit(47, packet_mask.get());

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyWrittenAndReadHeaders(kFlexfecHeaderSizes[2], packet_mask.get(),
                              kFlexfecPacketMaskSizes[2], *written_packet,
                              *read_packet);
}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadLargeUlpfecPacketHeaderMaskWithBit46ClearBit47Set) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  ClearBit(46, packet_mask.get());
  SetBit(47, packet_mask.get());

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyWrittenAndReadHeaders(kFlexfecHeaderSizes[2], packet_mask.get(),
                              kFlexfecPacketMaskSizes[2], *written_packet,
                              *read_packet);
}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBits46And47Set) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(46, packet_mask.get());
  SetBit(47, packet_mask.get());

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyWrittenAndReadHeaders(kFlexfecHeaderSizes[2], packet_mask.get(),
                              kFlexfecPacketMaskSizes[2], *written_packet,
                              *read_packet);
}

*/

}  // namespace webrtc
