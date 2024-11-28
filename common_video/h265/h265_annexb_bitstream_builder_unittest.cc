/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/h265_annexb_bitstream_builder.h"

#include <stdint.h>

#include "api/array_view.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr uint64_t kTestPattern = 0xfedcba0987654321;

uint64_t GetDataFromBuffer(rtc::ArrayView<const uint8_t> buffer,
                           uint64_t num_bits) {
  uint64_t got = 0;
  size_t index = 0;
  while (num_bits > 8) {
    got |= (buffer[index] & 0xff);
    num_bits -= 8;
    got <<= (num_bits > 8 ? 8 : num_bits);
    index++;
  }
  if (num_bits > 0) {
    uint64_t temp = (buffer[index] & 0xff);
    temp >>= (8 - num_bits);
    got |= temp;
  }
  return got;
}

uint64_t AlignUpToBytes(uint64_t num_bits) {
  return (num_bits + 7) / 8;
}

}  // namespace

class H265AnnexBBitstreamBuilderAppendBitsTest
    : public ::testing::TestWithParam<uint64_t> {};

TEST_P(H265AnnexBBitstreamBuilderAppendBitsTest, AppendAndVerifyBits) {
  H265AnnexBBitstreamBuilder b;
  uint64_t num_bits = GetParam();
  ASSERT_LE(num_bits, 64u);
  uint64_t num_bytes = AlignUpToBytes(num_bits);

  b.AppendBits(num_bits, kTestPattern);
  b.FlushReg();

  EXPECT_EQ(b.BytesInBuffer(), num_bytes);

  rtc::ArrayView<const uint8_t> ptr = b.data();
  uint64_t got = GetDataFromBuffer(ptr, num_bits);
  uint64_t expected = kTestPattern;

  if (num_bits < 64) {
    expected &= ((1ull << num_bits) - 1);
  }

  EXPECT_EQ(got, expected) << std::hex << "0x" << got << " vs 0x" << expected;
}

TEST_F(H265AnnexBBitstreamBuilderAppendBitsTest, VerifyFlushAndBitsInBuffer) {
  H265AnnexBBitstreamBuilder b;
  uint64_t num_bits = 20;
  uint64_t num_bytes = AlignUpToBytes(num_bits);

  b.AppendBits(num_bits, kTestPattern);
  b.Flush();

  EXPECT_EQ(b.BytesInBuffer(), num_bytes);
  EXPECT_EQ(b.BitsInBuffer(), num_bits);

  rtc::ArrayView<const uint8_t> ptr = b.data();
  uint64_t got = GetDataFromBuffer(ptr, num_bits);
  uint64_t expected = kTestPattern;
  expected &= ((1ull << num_bits) - 1);

  EXPECT_EQ(got, expected) << std::hex << "0x" << got << " vs 0x" << expected;
}

INSTANTIATE_TEST_SUITE_P(AppendNumBits,
                         H265AnnexBBitstreamBuilderAppendBitsTest,
                         ::testing::Range(static_cast<uint64_t>(1),
                                          static_cast<uint64_t>(65)));
}  // namespace webrtc
