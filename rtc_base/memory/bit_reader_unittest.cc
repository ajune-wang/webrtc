/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/memory/bit_reader.h"

#include <limits>

#include "rtc_base/byte_buffer.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(BitReaderTest, ConsumeBits) {
  const uint8_t bytes[32] = {0};
  BitReader reader(bytes);

  int total_bits = 32 * 8;
  EXPECT_EQ(reader.RemainingBitCount(), total_bits);
  reader.ConsumeBits(3);
  total_bits -= 3;
  EXPECT_EQ(reader.RemainingBitCount(), total_bits);
  reader.ConsumeBits(3);
  total_bits -= 3;
  EXPECT_EQ(reader.RemainingBitCount(), total_bits);
  reader.ConsumeBits(15);
  total_bits -= 15;
  EXPECT_EQ(reader.RemainingBitCount(), total_bits);
  reader.ConsumeBits(67);
  total_bits -= 67;
  EXPECT_EQ(reader.RemainingBitCount(), total_bits);
  EXPECT_TRUE(reader.Ok());

  reader.ConsumeBits(32 * 8);
  EXPECT_FALSE(reader.Ok());
  EXPECT_LT(reader.RemainingBitCount(), 0);
}

TEST(BitReaderTest, ReadBit) {
  const uint8_t bytes[] = {0b0100'0001, 0b1011'0001};
  BitReader reader(bytes);
  // First byte.
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 1);
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 0);

  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 1);

  // Second byte.
  EXPECT_EQ(reader.ReadBit(), 1);
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 1);
  EXPECT_EQ(reader.ReadBit(), 1);

  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 1);

  EXPECT_TRUE(reader.Ok());
  // Try to read beyound the buffer.
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitReaderTest, ReadBytesAligned) {
  const uint8_t bytes[] = {0x0A, 0xBC, 0xDE, 0xF1, 0x23, 0x45, 0x67, 0x89};
  BitReader reader(bytes);
  EXPECT_EQ(reader.Read<uint8_t>(), 0x0Au);
  EXPECT_EQ(reader.Read<uint8_t>(), 0xBCu);
  EXPECT_EQ(reader.Read<uint16_t>(), 0xDEF1u);
  EXPECT_EQ(reader.Read<uint32_t>(), 0x23456789u);
  EXPECT_TRUE(reader.Ok());
}

TEST(BitReaderTest, ReadBytesOffset4) {
  const uint8_t bytes[] = {0x0A, 0xBC, 0xDE, 0xF1, 0x23,
                           0x45, 0x67, 0x89, 0x0A};
  BitReader reader(bytes);
  reader.ConsumeBits(4);

  EXPECT_EQ(reader.Read<uint8_t>(), 0xABu);
  EXPECT_EQ(reader.Read<uint8_t>(), 0xCDu);
  EXPECT_EQ(reader.Read<uint16_t>(), 0xEF12u);
  EXPECT_EQ(reader.Read<uint32_t>(), 0x34567890u);
  EXPECT_TRUE(reader.Ok());
}

TEST(BitReaderTest, ReadBytesOffset3) {
  // The pattern we'll check against is counting down from 0b1111. It looks
  // weird here because it's all offset by 3.
  // Byte pattern is:
  //    56701234
  //  0b00011111,
  //  0b11011011,
  //  0b10010111,
  //  0b01010011,
  //  0b00001110,
  //  0b11001010,
  //  0b10000110,
  //  0b01000010
  //       xxxxx <-- last 5 bits unused.

  // The bytes. It almost looks like counting down by two at a time, except the
  // jump at 5->3->0, since that's when the high bit is turned off.
  const uint8_t bytes[] = {0x1F, 0xDB, 0x97, 0x53, 0x0E, 0xCA, 0x86, 0x42};

  BitReader reader(bytes);
  reader.ConsumeBits(3);
  EXPECT_EQ(reader.Read<uint8_t>(), 0xFEu);
  EXPECT_EQ(reader.Read<uint16_t>(), 0xDCBAu);
  EXPECT_EQ(reader.Read<uint32_t>(), 0x98765432u);
  EXPECT_TRUE(reader.Ok());

  // 5 bits left unread. Not enough to read a uint8_t.
  EXPECT_EQ(reader.RemainingBitCount(), 5);
  EXPECT_EQ(reader.Read<uint8_t>(), 0);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitReaderTest, ReadBits) {
  const uint8_t bytes[] = {0b010'01'101, 0b0011'00'1'0};
  BitReader reader(bytes);
  EXPECT_EQ(reader.ReadBits(3), 0b010u);
  EXPECT_EQ(reader.ReadBits(2), 0b01u);
  EXPECT_EQ(reader.ReadBits(7), 0b101'0011u);
  EXPECT_EQ(reader.ReadBits(2), 0b00u);
  EXPECT_EQ(reader.ReadBits(1), 0b1u);
  EXPECT_EQ(reader.ReadBits(1), 0b0u);
  EXPECT_TRUE(reader.Ok());

  EXPECT_EQ(reader.ReadBits(1), 0u);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitReaderTest, ReadZeroBits) {
  BitReader reader(rtc::ArrayView<const uint8_t>(nullptr, 0));

  EXPECT_EQ(reader.ReadBits(0), 0u);
  EXPECT_TRUE(reader.Ok());
}

TEST(BitReaderTest, ReadBitFromEmptyArray) {
  BitReader reader(rtc::ArrayView<const uint8_t>(nullptr, 0));

  // Trying to read from the empty array shouldn't derefernce the pointer,
  // i.e. shouldn't crash.
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitReaderTest, ReadBitsFromEmptyArray) {
  BitReader reader(rtc::ArrayView<const uint8_t>(nullptr, 0));

  // Trying to read from the empty array shouldn't derefernce the pointer,
  // i.e. shouldn't crash.
  EXPECT_EQ(reader.ReadBits(1), 0u);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitReaderTest, ReadBits64) {
  const uint8_t bytes[] = {0x4D, 0x32, 0xAB, 0x54, 0x00, 0xFF, 0xFE, 0x01,
                           0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89};
  BitReader reader(bytes);

  EXPECT_EQ(reader.ReadBits(33), 0x4D32AB5400FFFE01u >> (64 - 33));

  constexpr uint64_t kMask31Bits = (1ull << 32) - 1;
  EXPECT_EQ(reader.ReadBits(31), 0x4D32AB5400FFFE01ull & kMask31Bits);

  EXPECT_EQ(reader.ReadBits(64), 0xABCDEF0123456789ull);
  EXPECT_TRUE(reader.Ok());

  // Nothing more to read.
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitReaderTest, ReadNonSymmetricSameNumberOfBitsWhenNumValuesPowerOf2) {
  const uint8_t bytes[2] = {0xf3, 0xa0};
  BitReader reader(bytes);

  ASSERT_EQ(reader.RemainingBitCount(), 16);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/1 << 4), 0xfu);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/1 << 4), 0x3u);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/1 << 4), 0xau);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/1 << 4), 0x0u);
  EXPECT_EQ(reader.RemainingBitCount(), 0);
  EXPECT_TRUE(reader.Ok());
}

TEST(BitReaderTest, ReadNonSymmetricOnlyValueConsumesNoBits) {
  const uint8_t bytes[2] = {};
  BitReader reader(bytes);

  ASSERT_EQ(reader.RemainingBitCount(), 16);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/1), 0u);
  EXPECT_EQ(reader.RemainingBitCount(), 16);
}

uint64_t GolombEncoded(uint32_t val) {
  val++;
  uint32_t bit_counter = val;
  uint64_t bit_count = 0;
  while (bit_counter > 0) {
    bit_count++;
    bit_counter >>= 1;
  }
  return static_cast<uint64_t>(val) << (64 - (bit_count * 2 - 1));
}

TEST(BitBufferTest, GolombUint32Values) {
  rtc::ByteBufferWriter writer;
  writer.Resize(16);
  rtc::ArrayView<const uint8_t> buffer(
      reinterpret_cast<const uint8_t*>(writer.Data()), writer.Capacity());
  // Test over the uint32_t range with a large enough step that the test doesn't
  // take forever. Around 20,000 iterations should do.
  const int kStep = std::numeric_limits<uint32_t>::max() / 20000;
  for (uint32_t i = 0; i < std::numeric_limits<uint32_t>::max() - kStep;
       i += kStep) {
    uint64_t encoded_val = GolombEncoded(i);
    writer.Clear();
    writer.WriteUInt64(encoded_val);
    BitReader reader(buffer);
    EXPECT_EQ(reader.ReadExponentialGolomb(), i);
  }
}

TEST(BitBufferTest, SignedGolombValues) {
  uint8_t golomb_bits[] = {
      0x80,  // 1
      0x40,  // 010
      0x60,  // 011
      0x20,  // 00100
      0x38,  // 00111
  };
  int32_t expected[] = {0, 1, -1, 2, -3};
  for (size_t i = 0; i < sizeof(golomb_bits); ++i) {
    BitReader buffer(rtc::MakeArrayView(&golomb_bits[i], 1));
    EXPECT_EQ(buffer.ReadSignedExponentialGolomb(), expected[i])
        << "Mismatch in expected/decoded value for golomb_bits[" << i
        << "]: " << static_cast<int>(golomb_bits[i]);
  }
}

TEST(BitBufferTest, NoGolombOverread) {
  const uint8_t bytes[] = {0x00, 0xFF, 0xFF};
  // Make sure the bit buffer correctly enforces byte length on golomb reads.
  // If it didn't, the above buffer would be valid at 3 bytes.
  BitReader reader1(rtc::MakeArrayView(bytes, 1));
  EXPECT_EQ(reader1.ReadExponentialGolomb(), 0u);
  EXPECT_FALSE(reader1.Ok());

  BitReader reader2(rtc::MakeArrayView(bytes, 1));
  EXPECT_EQ(reader2.ReadExponentialGolomb(), 0u);
  EXPECT_FALSE(reader2.Ok());

  BitReader reader3(bytes);
  // Golomb should have read 9 bits, so 0x01FF, and since it is golomb, the
  // result is 0x01FF - 1 = 0x01FE.
  EXPECT_EQ(reader2.ReadExponentialGolomb(), 0x01FEu);
  EXPECT_TRUE(reader2.Ok());
}

}  // namespace
}  // namespace webrtc
