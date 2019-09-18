/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/divide_round.h"

#include <limits>

#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(DivideRoundUpTest, CanBeUsedAsConstexpr) {
  static_assert(DivideRoundUp(5, 1) == 5, "");
  static_assert(DivideRoundUp(5, 2) == 3, "");
}

TEST(DivideRoundUpTest, ReturnsZeroForZeroDividend) {
  EXPECT_EQ(DivideRoundUp(uint8_t{0}, 1), 0);
  EXPECT_EQ(DivideRoundUp(uint8_t{0}, 3), 0);
  EXPECT_EQ(DivideRoundUp(int{0}, 1), 0);
  EXPECT_EQ(DivideRoundUp(int{0}, 3), 0);
}

TEST(DivideRoundUpTest, WorksForMaxDividend) {
  EXPECT_EQ(DivideRoundUp(uint8_t{255}, 2), 128);
  EXPECT_EQ(DivideRoundUp(std::numeric_limits<int>::max(), 2),
            std::numeric_limits<int>::max() / 2 +
                (std::numeric_limits<int>::max() % 2));
}

TEST(DivideRoundUpTest, PreservesType) {
  uint8_t uint_dividend = 255;
  int64_t int_dividend = 255;
  uint32_t uint_divisor = 3;
  int16_t int_divisor = 2;

  auto round_up_uu = DivideRoundUp(uint_dividend, uint_divisor);
  auto round_up_us = DivideRoundUp(uint_dividend, int_divisor);
  auto round_up_su = DivideRoundUp(int_dividend, uint_divisor);
  auto round_up_ss = DivideRoundUp(int_dividend, int_divisor);
  static_assert(std::is_same<decltype(round_up_uu),
                             decltype(uint_dividend / uint_divisor)>::value,
                "");
  static_assert(std::is_same<decltype(round_up_us),
                             decltype(uint_dividend / int_divisor)>::value,
                "");
  static_assert(std::is_same<decltype(round_up_su),
                             decltype(int_dividend / uint_divisor)>::value,
                "");
  static_assert(std::is_same<decltype(round_up_ss),
                             decltype(int_dividend / int_divisor)>::value,
                "");
}

TEST(DivideRoundToNearestTest, CanBeUsedAsConstexpr) {
  static constexpr int kOne = DivideRoundToNearest(5, 4);
  static constexpr int kTwo = DivideRoundToNearest(7, 4);
  static_assert(kOne == 1, "");
  static_assert(kTwo == 2, "");
}

TEST(DivideRoundToNearestTest, DivideByOddNumber) {
  EXPECT_EQ(DivideRoundToNearest(0, 3), 0);
  EXPECT_EQ(DivideRoundToNearest(1, 3), 0);
  EXPECT_EQ(DivideRoundToNearest(2, 3), 1);
  EXPECT_EQ(DivideRoundToNearest(3, 3), 1);
  EXPECT_EQ(DivideRoundToNearest(4, 3), 1);
  EXPECT_EQ(DivideRoundToNearest(5, 3), 2);
  EXPECT_EQ(DivideRoundToNearest(6, 3), 2);
}

TEST(DivideRoundToNearestTest, DivideByEvenNumberTieRoundsUp) {
  EXPECT_EQ(DivideRoundToNearest(0, 4), 0);
  EXPECT_EQ(DivideRoundToNearest(1, 4), 0);
  EXPECT_EQ(DivideRoundToNearest(2, 4), 1);
  EXPECT_EQ(DivideRoundToNearest(3, 4), 1);
  EXPECT_EQ(DivideRoundToNearest(4, 4), 1);
  EXPECT_EQ(DivideRoundToNearest(5, 4), 1);
  EXPECT_EQ(DivideRoundToNearest(6, 4), 2);
  EXPECT_EQ(DivideRoundToNearest(7, 4), 2);
}

TEST(DivideRoundToNearestTest, LargeDivisor) {
  EXPECT_EQ(DivideRoundToNearest(std::numeric_limits<int>::max() - 1,
                                 std::numeric_limits<int>::max()),
            1);
}

TEST(DivideRoundToNearestTest, DivideSmallTypeByLargeType) {
  uint8_t small = 0xff;
  uint16_t large = 0xffff;
  EXPECT_EQ(DivideRoundToNearest(small, large), 0);
}

TEST(DivideRoundToNearestTest, PreservesType) {
  uint8_t uint_dividend = 255;
  int64_t int_dividend = 255;
  uint16_t uint_divisor = 3;
  int16_t int_divisor = 2;

  auto round_up_uu = DivideRoundToNearest(uint_dividend, uint_divisor);
  auto round_up_us = DivideRoundToNearest(uint_dividend, int_divisor);
  auto round_up_su = DivideRoundToNearest(int_dividend, uint_divisor);
  auto round_up_ss = DivideRoundToNearest(int_dividend, int_divisor);
  static_assert(std::is_same<decltype(round_up_uu),
                             decltype(uint_dividend / uint_divisor)>::value,
                "");
  static_assert(std::is_same<decltype(round_up_us),
                             decltype(uint_dividend / int_divisor)>::value,
                "");
  static_assert(std::is_same<decltype(round_up_su),
                             decltype(int_dividend / uint_divisor)>::value,
                "");
  static_assert(std::is_same<decltype(round_up_ss),
                             decltype(int_dividend / int_divisor)>::value,
                "");
}

}  // namespace
}  // namespace webrtc
