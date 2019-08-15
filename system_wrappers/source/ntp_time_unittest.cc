/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/ntp_time.h"

#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr uint32_t kNtpSec = 0x12345678;
constexpr uint32_t kNtpFrac = 0x23456789;

constexpr uint64_t kYear2035UnixTimeMs = 2051222400000;

constexpr int64_t kUnixEpochInNtpTimeSecs = 2208988800;
constexpr int64_t kOneSecQ32x32 = uint64_t{1} << 32;
constexpr int64_t kOneMsQ32x32 = 4294967;

TEST(NtpTimeTest, NoValueMeansInvalid) {
  NtpTime ntp;
  EXPECT_FALSE(ntp.Valid());
}

TEST(NtpTimeTest, CanResetValue) {
  NtpTime ntp(kNtpSec, kNtpFrac);
  EXPECT_TRUE(ntp.Valid());
  ntp.Reset();
  EXPECT_FALSE(ntp.Valid());
}

TEST(NtpTimeTest, CanGetWhatIsSet) {
  NtpTime ntp;
  ntp.Set(kNtpSec, kNtpFrac);
  EXPECT_EQ(kNtpSec, ntp.seconds());
  EXPECT_EQ(kNtpFrac, ntp.fractions());
}

TEST(NtpTimeTest, SetIsSameAs2ParameterConstructor) {
  NtpTime ntp1(kNtpSec, kNtpFrac);
  NtpTime ntp2;
  EXPECT_NE(ntp1, ntp2);

  ntp2.Set(kNtpSec, kNtpFrac);
  EXPECT_EQ(ntp1, ntp2);
}

TEST(NtpTimeTest, ToMsMeansToNtpMilliseconds) {
  SimulatedClock clock(0x123456789abc);

  NtpTime ntp = clock.CurrentNtpTime();
  EXPECT_EQ(ntp.ToMs(), Clock::NtpToMs(ntp.seconds(), ntp.fractions()));
  EXPECT_EQ(ntp.ToMs(), clock.CurrentNtpInMilliseconds());
}

TEST(NtpTimeTest, CanExplicitlyConvertToAndFromUint64) {
  uint64_t untyped_time = 0x123456789;
  NtpTime time(untyped_time);
  EXPECT_EQ(untyped_time, static_cast<uint64_t>(time));
  EXPECT_EQ(NtpTime(0x12345678, 0x90abcdef), NtpTime(0x1234567890abcdef));
}

TEST(NtpTimeTest, VerifyNtpTimeUQ32x32ToUnixTimeMsNearNtpEpoch) {
  constexpr int64_t kUnixTimeMs = -kUnixEpochInNtpTimeSecs * 1000;

  // NTP Epoch
  EXPECT_EQ(NtpTimeUQ32x32ToUnixTimeMs(0), kUnixTimeMs);

  // NTP Epoch + 1 second
  EXPECT_EQ(NtpTimeUQ32x32ToUnixTimeMs(kOneSecQ32x32), kUnixTimeMs + 1000);

  // NTP Epoch + 1 millisecond
  EXPECT_EQ(NtpTimeUQ32x32ToUnixTimeMs(kOneMsQ32x32), kUnixTimeMs + 1);
}

TEST(NtpTimeTest, VerifyNtpTimeUQ32x32ToUnixTimeMsNearNtpMax) {
  constexpr int64_t kUnixTimeMs =
      ((uint64_t{1} << 32) - kUnixEpochInNtpTimeSecs) * 1000;

  EXPECT_EQ(NtpTimeUQ32x32ToUnixTimeMs(~uint64_t{0}), kUnixTimeMs);
}

TEST(NtpTimeTest, VerifyUnixTimeMsToNtpTimeUQ32x32NearUnixEpoch) {
  constexpr uint64_t kNtpTimeUQ32x32 = kUnixEpochInNtpTimeSecs << 32;

  // Unix Epoch
  EXPECT_EQ(UnixTimeMsToNtpTimeUQ32x32(0), kNtpTimeUQ32x32);

  // Unix Epoch + 1 second
  EXPECT_EQ(UnixTimeMsToNtpTimeUQ32x32(1000), kNtpTimeUQ32x32 + kOneSecQ32x32);

  // Unix Epoch - 1 second
  EXPECT_EQ(UnixTimeMsToNtpTimeUQ32x32(-1000), kNtpTimeUQ32x32 - kOneSecQ32x32);

  // Unix Epoch + 1 millisecond
  EXPECT_EQ(UnixTimeMsToNtpTimeUQ32x32(1), kNtpTimeUQ32x32 + kOneMsQ32x32);

  // Unix Epoch - 1 millisecond
  EXPECT_EQ(UnixTimeMsToNtpTimeUQ32x32(-1), kNtpTimeUQ32x32 - kOneMsQ32x32);
}

TEST(NtpTimeTest, VerifyUnixTimeMsToNtpTimeUQ32x32RoundTrip) {
  for (int sign : {+1, -1}) {
    for (int64_t i = 0; i <= 2000; ++i) {
      int64_t unix_time_ms = sign * (kYear2035UnixTimeMs + i);
      uint64_t ntp_time_uq32x32 = UnixTimeMsToNtpTimeUQ32x32(unix_time_ms);

      EXPECT_EQ(NtpTimeUQ32x32ToUnixTimeMs(ntp_time_uq32x32), unix_time_ms)
          << "sign = " << ((sign == 1) ? "+1" : "-1") << ", i = " << i
          << ", unix_time_ms = " << unix_time_ms
          << ", ntp_time_uq32x32 = " << ntp_time_uq32x32;
    }
  }
}

TEST(NtpTimeTest, VerifyDurationMsToDurationQ32x32NearZero) {
  // Zero
  EXPECT_EQ(DurationMsToDurationQ32x32(0), 0);

  // Zero + 1 second
  EXPECT_EQ(DurationMsToDurationQ32x32(1000), kOneSecQ32x32);

  // Zero - 1 second
  EXPECT_EQ(DurationMsToDurationQ32x32(-1000), -kOneSecQ32x32);

  // Zero + 1 millisecond
  EXPECT_EQ(DurationMsToDurationQ32x32(1), kOneMsQ32x32);

  // Zero - 1 millisecond
  EXPECT_EQ(DurationMsToDurationQ32x32(-1), -kOneMsQ32x32);
}

TEST(NtpTimeTest, VerifyDurationMsToDurationQ32x32RoundTrip) {
  for (int sign : {+1, -1}) {
    for (int64_t i = 0; i <= 2000; ++i) {
      int64_t duration_ms = sign * (int64_t{365} * 24 * 60 * 60 * 1000 + i);
      int64_t duration_q32x32 = DurationMsToDurationQ32x32(duration_ms);

      EXPECT_EQ(DurationQ32x32ToDurationMs(duration_q32x32), duration_ms)
          << "sign = " << ((sign == 1) ? "+1" : "-1") << ", i = " << i
          << ", duration_ms = " << duration_ms
          << ", duration_q32x32 = " << duration_q32x32;
    }
  }
}

TEST(NtpTimeTest, VerifyDurationQ32x32ToDurationMsNearZero) {
  // Zero
  EXPECT_EQ(DurationQ32x32ToDurationMs(0), 0);

  // Zero + 1 second
  EXPECT_EQ(DurationQ32x32ToDurationMs(kOneSecQ32x32), 1000);

  // Zero - 1 second
  EXPECT_EQ(DurationQ32x32ToDurationMs(-kOneSecQ32x32), -1000);

  // Zero + 1 millisecond
  EXPECT_EQ(DurationQ32x32ToDurationMs(kOneMsQ32x32), 1);

  // Zero - 1 millisecond
  EXPECT_EQ(DurationQ32x32ToDurationMs(-kOneMsQ32x32), -1);
}

}  // namespace
}  // namespace webrtc
