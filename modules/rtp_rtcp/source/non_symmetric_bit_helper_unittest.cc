/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/non_symmetric_bit_helper.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;

TEST(NonSymmetricBitHelperTest, WritesSameNumberOfBitsWhenNumbValuesPowerOf2) {
  NonSymmetricBitHelper helper(/*num_values=*/1 << 4);

  uint8_t bytes[2] = {0};
  rtc::BitBufferWriter writer(bytes, 2);

  ASSERT_EQ(writer.RemainingBitCount(), 16u);
  EXPECT_TRUE(helper.Write(&writer, 0xf));
  ASSERT_EQ(writer.RemainingBitCount(), 12u);
  EXPECT_TRUE(helper.Write(&writer, 0x3));
  ASSERT_EQ(writer.RemainingBitCount(), 8u);
  EXPECT_TRUE(helper.Write(&writer, 0xa));
  ASSERT_EQ(writer.RemainingBitCount(), 4u);
  EXPECT_TRUE(helper.Write(&writer, 0x0));
  ASSERT_EQ(writer.RemainingBitCount(), 0u);

  uint32_t values[4];
  rtc::BitBuffer reader(bytes, 2);
  EXPECT_TRUE(reader.ReadBits(&values[0], 4));
  EXPECT_TRUE(reader.ReadBits(&values[1], 4));
  EXPECT_TRUE(reader.ReadBits(&values[2], 4));
  EXPECT_TRUE(reader.ReadBits(&values[3], 4));

  EXPECT_THAT(values, ElementsAre(0xf, 0x3, 0xa, 0x0));
}

TEST(NonSymmetricBitHelperTest, ReadsSameNumberOfBitsWhenNumbValuesPowerOf2) {
  NonSymmetricBitHelper helper(/*num_values=*/1 << 4);

  const uint8_t bytes[2] = {0xf3, 0xa0};
  rtc::BitBuffer reader(bytes, 2);

  uint32_t values[4];
  ASSERT_EQ(reader.RemainingBitCount(), 16u);
  EXPECT_TRUE(helper.Read(&reader, &values[0]));
  EXPECT_TRUE(helper.Read(&reader, &values[1]));
  EXPECT_TRUE(helper.Read(&reader, &values[2]));
  EXPECT_TRUE(helper.Read(&reader, &values[3]));
  ASSERT_EQ(reader.RemainingBitCount(), 0u);

  EXPECT_THAT(values, ElementsAre(0xf, 0x3, 0xa, 0x0));
}

TEST(NonSymmetricBitHelperTest, ReadsMatchesWrites) {
  NonSymmetricBitHelper helper(/*num_values=*/6);

  uint8_t bytes[2] = {0};
  rtc::BitBufferWriter writer(bytes, 2);

  EXPECT_EQ(helper.BitSize(1), 2);
  EXPECT_EQ(helper.BitSize(2), 3);
  // Values [0, 1] can fit into two bit.
  ASSERT_EQ(writer.RemainingBitCount(), 16u);
  EXPECT_TRUE(helper.Write(&writer, 0));
  ASSERT_EQ(writer.RemainingBitCount(), 14u);
  EXPECT_TRUE(helper.Write(&writer, 1));
  ASSERT_EQ(writer.RemainingBitCount(), 12u);
  // Values [2, 5] require 3 bits.
  EXPECT_TRUE(helper.Write(&writer, 2));
  ASSERT_EQ(writer.RemainingBitCount(), 9u);
  EXPECT_TRUE(helper.Write(&writer, 3));
  ASSERT_EQ(writer.RemainingBitCount(), 6u);
  EXPECT_TRUE(helper.Write(&writer, 4));
  ASSERT_EQ(writer.RemainingBitCount(), 3u);
  EXPECT_TRUE(helper.Write(&writer, 5));
  ASSERT_EQ(writer.RemainingBitCount(), 0u);

  rtc::BitBuffer reader(bytes, 2);
  uint32_t values[6];
  EXPECT_TRUE(helper.Read(&reader, &values[0]));
  EXPECT_TRUE(helper.Read(&reader, &values[1]));
  EXPECT_TRUE(helper.Read(&reader, &values[2]));
  EXPECT_TRUE(helper.Read(&reader, &values[3]));
  EXPECT_TRUE(helper.Read(&reader, &values[4]));
  EXPECT_TRUE(helper.Read(&reader, &values[5]));

  EXPECT_THAT(values, ElementsAre(0, 1, 2, 3, 4, 5));
}

}  // namespace
}  // namespace webrtc
