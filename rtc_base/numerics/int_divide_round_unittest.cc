/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/int_divide_round.h"

#include <limits>

#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(IntDivideRoundUpTest, CanBeUsedAsConstexpr) {
  static_assert(IntDivideRoundUp(5, 1) == 5, "");
  static_assert(IntDivideRoundUp(5, 2) == 3, "");
}

TEST(IntDivideRoundUpTest, ReturnsZeroForZeroDividend) {
  EXPECT_EQ(IntDivideRoundUp(uint8_t{0}, 1), 0);
  EXPECT_EQ(IntDivideRoundUp(uint8_t{0}, 3), 0);
  EXPECT_EQ(IntDivideRoundUp(int{0}, 1), 0);
  EXPECT_EQ(IntDivideRoundUp(int{0}, 3), 0);
}

TEST(IntDivideRoundUpTest, WorksForMaxValues) {
  EXPECT_EQ(IntDivideRoundUp(uint8_t{255}, 2), 128);
  EXPECT_EQ(IntDivideRoundUp(std::numeric_limits<int>::max(), 2),
            std::numeric_limits<int>::max() / 2 +
                (std::numeric_limits<int>::max() % 2));
}

TEST(IntDivideRoundUpTest, PreservesType) {
  uint8_t uint_dividend = 255;
  int64_t int_dividend = 255;
  uint32_t uint_divisor = 3;
  int16_t int_divisor = 2;

  auto round_up_uu = IntDivideRoundUp(uint_dividend, uint_divisor);
  auto round_up_us = IntDivideRoundUp(uint_dividend, int_divisor);
  auto round_up_su = IntDivideRoundUp(int_dividend, uint_divisor);
  auto round_up_ss = IntDivideRoundUp(int_dividend, int_divisor);
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

TEST(IntDivideRoundToNearestTest, CanBeUsedAsConstexpr) {
  static constexpr int kOne = IntDivideRoundToNearest(5, 4);
  static constexpr int kTwo = IntDivideRoundToNearest(7, 4);
  EXPECT_EQ(kOne, 1);
  EXPECT_EQ(kTwo, 2);
}

TEST(IntDivideRoundToNearestTest, DivideByOddNumber) {
  EXPECT_EQ(IntDivideRoundToNearest(0, 3), 0);
  EXPECT_EQ(IntDivideRoundToNearest(1, 3), 0);
  EXPECT_EQ(IntDivideRoundToNearest(2, 3), 1);
  EXPECT_EQ(IntDivideRoundToNearest(3, 3), 1);
  EXPECT_EQ(IntDivideRoundToNearest(4, 3), 1);
  EXPECT_EQ(IntDivideRoundToNearest(5, 3), 2);
  EXPECT_EQ(IntDivideRoundToNearest(6, 3), 2);
}

TEST(IntDivideRoundToNearestTest, DivideByEvenNumberTieRoundsUp) {
  EXPECT_EQ(IntDivideRoundToNearest(0, 4), 0);
  EXPECT_EQ(IntDivideRoundToNearest(1, 4), 0);
  EXPECT_EQ(IntDivideRoundToNearest(2, 4), 1);
  EXPECT_EQ(IntDivideRoundToNearest(3, 4), 1);
  EXPECT_EQ(IntDivideRoundToNearest(4, 4), 1);
  EXPECT_EQ(IntDivideRoundToNearest(5, 4), 1);
  EXPECT_EQ(IntDivideRoundToNearest(6, 4), 2);
  EXPECT_EQ(IntDivideRoundToNearest(7, 4), 2);
}

TEST(IntDivideRoundToNearestTest, DivideMaxValue) {
  EXPECT_EQ(IntDivideRoundToNearest(uint8_t{255}, 10), 26);
  EXPECT_EQ(IntDivideRoundToNearest(std::numeric_limits<int>::max(), 2),
            IntDivideRoundUp(std::numeric_limits<int>::max(), 2));
}

TEST(IntDivideRoundToNearestTest, DivideSmallTypeByLargeType) {
  uint8_t small = 0xff;
  uint16_t large = 0xffff;
  EXPECT_EQ(IntDivideRoundToNearest(small, large), 0);
}

TEST(IntDivideRoundToNearestTest, PreservesType) {
  uint8_t uint_dividend = 255;
  int64_t int_dividend = 255;
  uint32_t uint_divisor = 3;
  int16_t int_divisor = 2;

  auto round_up_uu = IntDivideRoundToNearest(uint_dividend, uint_divisor);
  auto round_up_us = IntDivideRoundToNearest(uint_dividend, int_divisor);
  auto round_up_su = IntDivideRoundToNearest(int_dividend, uint_divisor);
  auto round_up_ss = IntDivideRoundToNearest(int_dividend, int_divisor);
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
