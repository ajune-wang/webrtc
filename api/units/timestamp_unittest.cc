/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <limits>

#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
TEST(TimestampTest, ConstExpr) {
  constexpr int64_t kValue = 12345;
  constexpr Timestamp kTimestampInf = Timestamp::PlusInfinity();
  static_assert(kTimestampInf.IsInfinite(), "");
  static_assert(kTimestampInf.MillisecondsOr(-1) == -1, "");

  constexpr Timestamp kTimestampSeconds = Timestamp::Seconds(kValue);
  constexpr Timestamp kTimestampMs = Timestamp::Milliseconds(kValue);
  constexpr Timestamp kTimestampUs = Timestamp::Microseconds(kValue);

  static_assert(kTimestampSeconds.SecondsOr(0) == kValue, "");
  static_assert(kTimestampMs.MillisecondsOr(0) == kValue, "");
  static_assert(kTimestampUs.MicrosecondsOr(0) == kValue, "");

  static_assert(kTimestampMs > kTimestampUs, "");

  EXPECT_EQ(kTimestampSeconds.Seconds(), kValue);
  EXPECT_EQ(kTimestampMs.Milliseconds(), kValue);
  EXPECT_EQ(kTimestampUs.Microseconds(), kValue);
}

TEST(TimestampTest, GetBackSameValues) {
  const int64_t kValue = 499;
  EXPECT_EQ(Timestamp::Milliseconds(kValue).Milliseconds(), kValue);
  EXPECT_EQ(Timestamp::Microseconds(kValue).Microseconds(), kValue);
  EXPECT_EQ(Timestamp::Seconds(kValue).Seconds(), kValue);
}

TEST(TimestampTest, GetDifferentPrefix) {
  const int64_t kValue = 3000000;
  EXPECT_EQ(Timestamp::Microseconds(kValue).Seconds(), kValue / 1000000);
  EXPECT_EQ(Timestamp::Milliseconds(kValue).Seconds(), kValue / 1000);
  EXPECT_EQ(Timestamp::Microseconds(kValue).Milliseconds(), kValue / 1000);

  EXPECT_EQ(Timestamp::Milliseconds(kValue).Microseconds(), kValue * 1000);
  EXPECT_EQ(Timestamp::Seconds(kValue).Milliseconds(), kValue * 1000);
  EXPECT_EQ(Timestamp::Seconds(kValue).Microseconds(), kValue * 1000000);
}

TEST(TimestampTest, IdentityChecks) {
  const int64_t kValue = 3000;

  EXPECT_TRUE(Timestamp::PlusInfinity().IsInfinite());
  EXPECT_TRUE(Timestamp::MinusInfinity().IsInfinite());
  EXPECT_FALSE(Timestamp::Milliseconds(kValue).IsInfinite());

  EXPECT_FALSE(Timestamp::PlusInfinity().IsFinite());
  EXPECT_FALSE(Timestamp::MinusInfinity().IsFinite());
  EXPECT_TRUE(Timestamp::Milliseconds(kValue).IsFinite());

  EXPECT_TRUE(Timestamp::PlusInfinity().IsPlusInfinity());
  EXPECT_FALSE(Timestamp::MinusInfinity().IsPlusInfinity());

  EXPECT_TRUE(Timestamp::MinusInfinity().IsMinusInfinity());
  EXPECT_FALSE(Timestamp::PlusInfinity().IsMinusInfinity());
}

TEST(TimestampTest, ComparisonOperators) {
  const int64_t kSmall = 450;
  const int64_t kLarge = 451;

  EXPECT_EQ(Timestamp::PlusInfinity(), Timestamp::PlusInfinity());
  EXPECT_GE(Timestamp::PlusInfinity(), Timestamp::PlusInfinity());
  EXPECT_GT(Timestamp::PlusInfinity(), Timestamp::Milliseconds(kLarge));
  EXPECT_EQ(Timestamp::Milliseconds(kSmall), Timestamp::Milliseconds(kSmall));
  EXPECT_LE(Timestamp::Milliseconds(kSmall), Timestamp::Milliseconds(kSmall));
  EXPECT_GE(Timestamp::Milliseconds(kSmall), Timestamp::Milliseconds(kSmall));
  EXPECT_NE(Timestamp::Milliseconds(kSmall), Timestamp::Milliseconds(kLarge));
  EXPECT_LE(Timestamp::Milliseconds(kSmall), Timestamp::Milliseconds(kLarge));
  EXPECT_LT(Timestamp::Milliseconds(kSmall), Timestamp::Milliseconds(kLarge));
  EXPECT_GE(Timestamp::Milliseconds(kLarge), Timestamp::Milliseconds(kSmall));
  EXPECT_GT(Timestamp::Milliseconds(kLarge), Timestamp::Milliseconds(kSmall));
}

TEST(TimestampTest, CanBeInititializedFromLargeInt) {
  const int kMaxInt = std::numeric_limits<int>::max();
  EXPECT_EQ(Timestamp::Seconds(kMaxInt).Microseconds(),
            static_cast<int64_t>(kMaxInt) * 1000000);
  EXPECT_EQ(Timestamp::Milliseconds(kMaxInt).Microseconds(),
            static_cast<int64_t>(kMaxInt) * 1000);
}

TEST(TimestampTest, ConvertsToAndFromDouble) {
  const int64_t kMicros = 17017;
  const double kMicrosDouble = kMicros;
  const double kMillisDouble = kMicros * 1e-3;
  const double kSecondsDouble = kMillisDouble * 1e-3;

  EXPECT_EQ(Timestamp::Microseconds(kMicros).Seconds<double>(), kSecondsDouble);
  EXPECT_EQ(Timestamp::Seconds(kSecondsDouble).Microseconds(), kMicros);

  EXPECT_EQ(Timestamp::Microseconds(kMicros).Milliseconds<double>(),
            kMillisDouble);
  EXPECT_EQ(Timestamp::Milliseconds(kMillisDouble).Microseconds(), kMicros);

  EXPECT_EQ(Timestamp::Microseconds(kMicros).Microseconds<double>(),
            kMicrosDouble);
  EXPECT_EQ(Timestamp::Microseconds(kMicrosDouble).Microseconds(), kMicros);

  const double kPlusInfinity = std::numeric_limits<double>::infinity();
  const double kMinusInfinity = -kPlusInfinity;

  EXPECT_EQ(Timestamp::PlusInfinity().Seconds<double>(), kPlusInfinity);
  EXPECT_EQ(Timestamp::MinusInfinity().Seconds<double>(), kMinusInfinity);
  EXPECT_EQ(Timestamp::PlusInfinity().Milliseconds<double>(), kPlusInfinity);
  EXPECT_EQ(Timestamp::MinusInfinity().Milliseconds<double>(), kMinusInfinity);
  EXPECT_EQ(Timestamp::PlusInfinity().Microseconds<double>(), kPlusInfinity);
  EXPECT_EQ(Timestamp::MinusInfinity().Microseconds<double>(), kMinusInfinity);

  EXPECT_TRUE(Timestamp::Seconds(kPlusInfinity).IsPlusInfinity());
  EXPECT_TRUE(Timestamp::Seconds(kMinusInfinity).IsMinusInfinity());
  EXPECT_TRUE(Timestamp::Milliseconds(kPlusInfinity).IsPlusInfinity());
  EXPECT_TRUE(Timestamp::Milliseconds(kMinusInfinity).IsMinusInfinity());
  EXPECT_TRUE(Timestamp::Microseconds(kPlusInfinity).IsPlusInfinity());
  EXPECT_TRUE(Timestamp::Microseconds(kMinusInfinity).IsMinusInfinity());
}

TEST(UnitConversionTest, TimestampAndTimeDeltaMath) {
  const int64_t kValueA = 267;
  const int64_t kValueB = 450;
  const Timestamp time_a = Timestamp::Milliseconds(kValueA);
  const Timestamp time_b = Timestamp::Milliseconds(kValueB);
  const TimeDelta delta_a = TimeDelta::Milliseconds(kValueA);
  const TimeDelta delta_b = TimeDelta::Milliseconds(kValueB);

  EXPECT_EQ((time_a - time_b), TimeDelta::Milliseconds(kValueA - kValueB));
  EXPECT_EQ((time_b - delta_a), Timestamp::Milliseconds(kValueB - kValueA));
  EXPECT_EQ((time_b + delta_a), Timestamp::Milliseconds(kValueB + kValueA));

  Timestamp mutable_time = time_a;
  mutable_time += delta_b;
  EXPECT_EQ(mutable_time, time_a + delta_b);
  mutable_time -= delta_b;
  EXPECT_EQ(mutable_time, time_a);
}

TEST(UnitConversionTest, InfinityOperations) {
  const int64_t kValue = 267;
  const Timestamp finite_time = Timestamp::Milliseconds(kValue);
  const TimeDelta finite_delta = TimeDelta::Milliseconds(kValue);
  EXPECT_TRUE((Timestamp::PlusInfinity() + finite_delta).IsInfinite());
  EXPECT_TRUE((Timestamp::PlusInfinity() - finite_delta).IsInfinite());
  EXPECT_TRUE((finite_time + TimeDelta::PlusInfinity()).IsInfinite());
  EXPECT_TRUE((finite_time - TimeDelta::MinusInfinity()).IsInfinite());
}
}  // namespace test
}  // namespace webrtc
