/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/units/time_delta.h"

#include <limits>

#include "test/gtest.h"

namespace webrtc {
namespace test {
TEST(TimeDeltaTest, ConstExpr) {
  constexpr int64_t kValue = -12345;
  constexpr TimeDelta kTimeDeltaZero = TimeDelta::Zero();
  constexpr TimeDelta kTimeDeltaPlusInf = TimeDelta::PlusInfinity();
  constexpr TimeDelta kTimeDeltaMinusInf = TimeDelta::MinusInfinity();
  static_assert(kTimeDeltaZero.IsZero(), "");
  static_assert(kTimeDeltaPlusInf.IsPlusInfinity(), "");
  static_assert(kTimeDeltaMinusInf.IsMinusInfinity(), "");
  static_assert(kTimeDeltaPlusInf.MillisecondsOr(-1) == -1, "");

  static_assert(kTimeDeltaPlusInf > kTimeDeltaZero, "");

  constexpr TimeDelta kTimeDeltaSeconds = TimeDelta::Seconds(kValue);
  constexpr TimeDelta kTimeDeltaMs = TimeDelta::Milliseconds(kValue);
  constexpr TimeDelta kTimeDeltaUs = TimeDelta::Microseconds(kValue);

  static_assert(kTimeDeltaSeconds.SecondsOr(0) == kValue, "");
  static_assert(kTimeDeltaMs.MillisecondsOr(0) == kValue, "");
  static_assert(kTimeDeltaUs.MicrosecondsOr(0) == kValue, "");
}

TEST(TimeDeltaTest, GetBackSameValues) {
  const int64_t kValue = 499;
  for (int sign = -1; sign <= 1; ++sign) {
    int64_t value = kValue * sign;
    EXPECT_EQ(TimeDelta::Milliseconds(value).Milliseconds(), value);
    EXPECT_EQ(TimeDelta::Microseconds(value).Microseconds(), value);
    EXPECT_EQ(TimeDelta::Seconds(value).Seconds(), value);
    EXPECT_EQ(TimeDelta::Seconds(value).Seconds(), value);
  }
  EXPECT_EQ(TimeDelta::Zero().Microseconds(), 0);
}

TEST(TimeDeltaTest, GetDifferentPrefix) {
  const int64_t kValue = 3000000;
  EXPECT_EQ(TimeDelta::Microseconds(kValue).Seconds(), kValue / 1000000);
  EXPECT_EQ(TimeDelta::Milliseconds(kValue).Seconds(), kValue / 1000);
  EXPECT_EQ(TimeDelta::Microseconds(kValue).Milliseconds(), kValue / 1000);

  EXPECT_EQ(TimeDelta::Milliseconds(kValue).Microseconds(), kValue * 1000);
  EXPECT_EQ(TimeDelta::Seconds(kValue).Milliseconds(), kValue * 1000);
  EXPECT_EQ(TimeDelta::Seconds(kValue).Microseconds(), kValue * 1000000);
}

TEST(TimeDeltaTest, IdentityChecks) {
  const int64_t kValue = 3000;
  EXPECT_TRUE(TimeDelta::Zero().IsZero());
  EXPECT_FALSE(TimeDelta::Milliseconds(kValue).IsZero());

  EXPECT_TRUE(TimeDelta::PlusInfinity().IsInfinite());
  EXPECT_TRUE(TimeDelta::MinusInfinity().IsInfinite());
  EXPECT_FALSE(TimeDelta::Zero().IsInfinite());
  EXPECT_FALSE(TimeDelta::Milliseconds(-kValue).IsInfinite());
  EXPECT_FALSE(TimeDelta::Milliseconds(kValue).IsInfinite());

  EXPECT_FALSE(TimeDelta::PlusInfinity().IsFinite());
  EXPECT_FALSE(TimeDelta::MinusInfinity().IsFinite());
  EXPECT_TRUE(TimeDelta::Milliseconds(-kValue).IsFinite());
  EXPECT_TRUE(TimeDelta::Milliseconds(kValue).IsFinite());
  EXPECT_TRUE(TimeDelta::Zero().IsFinite());

  EXPECT_TRUE(TimeDelta::PlusInfinity().IsPlusInfinity());
  EXPECT_FALSE(TimeDelta::MinusInfinity().IsPlusInfinity());

  EXPECT_TRUE(TimeDelta::MinusInfinity().IsMinusInfinity());
  EXPECT_FALSE(TimeDelta::PlusInfinity().IsMinusInfinity());
}

TEST(TimeDeltaTest, ComparisonOperators) {
  const int64_t kSmall = 450;
  const int64_t kLarge = 451;
  const TimeDelta small = TimeDelta::Milliseconds(kSmall);
  const TimeDelta large = TimeDelta::Milliseconds(kLarge);

  EXPECT_EQ(TimeDelta::Zero(), TimeDelta::Milliseconds(0));
  EXPECT_EQ(TimeDelta::PlusInfinity(), TimeDelta::PlusInfinity());
  EXPECT_EQ(small, TimeDelta::Milliseconds(kSmall));
  EXPECT_LE(small, TimeDelta::Milliseconds(kSmall));
  EXPECT_GE(small, TimeDelta::Milliseconds(kSmall));
  EXPECT_NE(small, TimeDelta::Milliseconds(kLarge));
  EXPECT_LE(small, TimeDelta::Milliseconds(kLarge));
  EXPECT_LT(small, TimeDelta::Milliseconds(kLarge));
  EXPECT_GE(large, TimeDelta::Milliseconds(kSmall));
  EXPECT_GT(large, TimeDelta::Milliseconds(kSmall));
  EXPECT_LT(TimeDelta::Zero(), small);
  EXPECT_GT(TimeDelta::Zero(), TimeDelta::Milliseconds(-kSmall));
  EXPECT_GT(TimeDelta::Zero(), TimeDelta::Milliseconds(-kSmall));

  EXPECT_GT(TimeDelta::PlusInfinity(), large);
  EXPECT_LT(TimeDelta::MinusInfinity(), TimeDelta::Zero());
}

TEST(TimeDeltaTest, Clamping) {
  const TimeDelta upper = TimeDelta::Milliseconds(800);
  const TimeDelta lower = TimeDelta::Milliseconds(100);
  const TimeDelta under = TimeDelta::Milliseconds(100);
  const TimeDelta inside = TimeDelta::Milliseconds(500);
  const TimeDelta over = TimeDelta::Milliseconds(1000);
  EXPECT_EQ(under.Clamped(lower, upper), lower);
  EXPECT_EQ(inside.Clamped(lower, upper), inside);
  EXPECT_EQ(over.Clamped(lower, upper), upper);

  TimeDelta mutable_delta = lower;
  mutable_delta.Clamp(lower, upper);
  EXPECT_EQ(mutable_delta, lower);
  mutable_delta = inside;
  mutable_delta.Clamp(lower, upper);
  EXPECT_EQ(mutable_delta, inside);
  mutable_delta = over;
  mutable_delta.Clamp(lower, upper);
  EXPECT_EQ(mutable_delta, upper);
}

TEST(TimeDeltaTest, CanBeInititializedFromLargeInt) {
  const int kMaxInt = std::numeric_limits<int>::max();
  EXPECT_EQ(TimeDelta::Seconds(kMaxInt).Microseconds(),
            static_cast<int64_t>(kMaxInt) * 1000000);
  EXPECT_EQ(TimeDelta::Milliseconds(kMaxInt).Microseconds(),
            static_cast<int64_t>(kMaxInt) * 1000);
}

TEST(TimeDeltaTest, ConvertsToAndFromDouble) {
  const int64_t kMicros = 17017;
  const double kNanosDouble = kMicros * 1e3;
  const double kMicrosDouble = kMicros;
  const double kMillisDouble = kMicros * 1e-3;
  const double kSecondsDouble = kMillisDouble * 1e-3;

  EXPECT_EQ(TimeDelta::Microseconds(kMicros).Seconds<double>(), kSecondsDouble);
  EXPECT_EQ(TimeDelta::Seconds(kSecondsDouble).Microseconds(), kMicros);

  EXPECT_EQ(TimeDelta::Microseconds(kMicros).Milliseconds<double>(),
            kMillisDouble);
  EXPECT_EQ(TimeDelta::Milliseconds(kMillisDouble).Microseconds(), kMicros);

  EXPECT_EQ(TimeDelta::Microseconds(kMicros).Microseconds<double>(),
            kMicrosDouble);
  EXPECT_EQ(TimeDelta::Microseconds(kMicrosDouble).Microseconds(), kMicros);

  EXPECT_NEAR(TimeDelta::Microseconds(kMicros).Nanoseconds<double>(),
              kNanosDouble, 1);

  const double kPlusInfinity = std::numeric_limits<double>::infinity();
  const double kMinusInfinity = -kPlusInfinity;

  EXPECT_EQ(TimeDelta::PlusInfinity().Seconds<double>(), kPlusInfinity);
  EXPECT_EQ(TimeDelta::MinusInfinity().Seconds<double>(), kMinusInfinity);
  EXPECT_EQ(TimeDelta::PlusInfinity().Milliseconds<double>(), kPlusInfinity);
  EXPECT_EQ(TimeDelta::MinusInfinity().Milliseconds<double>(), kMinusInfinity);
  EXPECT_EQ(TimeDelta::PlusInfinity().Microseconds<double>(), kPlusInfinity);
  EXPECT_EQ(TimeDelta::MinusInfinity().Microseconds<double>(), kMinusInfinity);
  EXPECT_EQ(TimeDelta::PlusInfinity().Nanoseconds<double>(), kPlusInfinity);
  EXPECT_EQ(TimeDelta::MinusInfinity().Nanoseconds<double>(), kMinusInfinity);

  EXPECT_TRUE(TimeDelta::Seconds(kPlusInfinity).IsPlusInfinity());
  EXPECT_TRUE(TimeDelta::Seconds(kMinusInfinity).IsMinusInfinity());
  EXPECT_TRUE(TimeDelta::Milliseconds(kPlusInfinity).IsPlusInfinity());
  EXPECT_TRUE(TimeDelta::Milliseconds(kMinusInfinity).IsMinusInfinity());
  EXPECT_TRUE(TimeDelta::Microseconds(kPlusInfinity).IsPlusInfinity());
  EXPECT_TRUE(TimeDelta::Microseconds(kMinusInfinity).IsMinusInfinity());
}

TEST(TimeDeltaTest, MathOperations) {
  const int64_t kValueA = 267;
  const int64_t kValueB = 450;
  const TimeDelta delta_a = TimeDelta::Milliseconds(kValueA);
  const TimeDelta delta_b = TimeDelta::Milliseconds(kValueB);
  EXPECT_EQ((delta_a + delta_b).Milliseconds(), kValueA + kValueB);
  EXPECT_EQ((delta_a - delta_b).Milliseconds(), kValueA - kValueB);

  const int32_t kInt32Value = 123;
  const double kFloatValue = 123.0;
  EXPECT_EQ((TimeDelta::Microseconds(kValueA) * kValueB).Microseconds(),
            kValueA * kValueB);
  EXPECT_EQ((TimeDelta::Microseconds(kValueA) * kInt32Value).Microseconds(),
            kValueA * kInt32Value);
  EXPECT_EQ((TimeDelta::Microseconds(kValueA) * kFloatValue).Microseconds(),
            kValueA * kFloatValue);

  EXPECT_EQ((delta_b / 10).Milliseconds(), kValueB / 10);
  EXPECT_EQ(delta_b / delta_a, static_cast<double>(kValueB) / kValueA);

  EXPECT_EQ(TimeDelta::Microseconds(-kValueA).Abs().Microseconds(), kValueA);
  EXPECT_EQ(TimeDelta::Microseconds(kValueA).Abs().Microseconds(), kValueA);

  TimeDelta mutable_delta = TimeDelta::Milliseconds(kValueA);
  mutable_delta += TimeDelta::Milliseconds(kValueB);
  EXPECT_EQ(mutable_delta, TimeDelta::Milliseconds(kValueA + kValueB));
  mutable_delta -= TimeDelta::Milliseconds(kValueB);
  EXPECT_EQ(mutable_delta, TimeDelta::Milliseconds(kValueA));
}

TEST(TimeDeltaTest, InfinityOperations) {
  const int64_t kValue = 267;
  const TimeDelta finite = TimeDelta::Milliseconds(kValue);
  EXPECT_TRUE((TimeDelta::PlusInfinity() + finite).IsPlusInfinity());
  EXPECT_TRUE((TimeDelta::PlusInfinity() - finite).IsPlusInfinity());
  EXPECT_TRUE((finite + TimeDelta::PlusInfinity()).IsPlusInfinity());
  EXPECT_TRUE((finite - TimeDelta::MinusInfinity()).IsPlusInfinity());

  EXPECT_TRUE((TimeDelta::MinusInfinity() + finite).IsMinusInfinity());
  EXPECT_TRUE((TimeDelta::MinusInfinity() - finite).IsMinusInfinity());
  EXPECT_TRUE((finite + TimeDelta::MinusInfinity()).IsMinusInfinity());
  EXPECT_TRUE((finite - TimeDelta::PlusInfinity()).IsMinusInfinity());
}
}  // namespace test
}  // namespace webrtc
