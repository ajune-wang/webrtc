/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/ntp_time.h"

#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {

template <typename ValueType, ValueType kDivisor>
ValueType DivideAndRound(ValueType value) {
  static_assert(kDivisor > 0, "Divisor must be positive.");

  ValueType quotient = value / kDivisor;
  ValueType remainder = value % kDivisor;

  if (remainder >= (kDivisor + 1) / 2) {
    return quotient + 1;
  }

  return quotient;
}

template <typename InputType,
          typename OutputType,
          InputType kInputSecondsDivisor,
          OutputType kOutputSecondsMultiplier,
          OutputType kOutputSecondsOffset>
OutputType ConvertFixedPointTime(InputType input) {
  static_assert(kInputSecondsDivisor > 0, "Divisor must be positive.");
  static_assert(kOutputSecondsMultiplier > 0, "Multiplier must be positive.");

  InputType input_seconds = input / kInputSecondsDivisor;
  InputType input_fractional = input % kInputSecondsDivisor;

  if (input_fractional < 0) {
    input_seconds -= 1;
    input_fractional += kInputSecondsDivisor;
  }

  OutputType output_seconds = input_seconds + kOutputSecondsOffset;
  OutputType output_fractional =
      DivideAndRound<OutputType, kInputSecondsDivisor>(
          input_fractional * kOutputSecondsMultiplier);

  return (output_seconds * kOutputSecondsMultiplier) + output_fractional;
}

}  // namespace

uint64_t UnixTimeMsToNtpTimeUQ32x32(int64_t unix_time_ms) {
  constexpr uint64_t kUnixEpochInNtpTimeSeconds = 2208988800ULL;

  return ConvertFixedPointTime<int64_t, uint64_t, 1000,
                               NtpTime::kFractionsPerSecond,
                               kUnixEpochInNtpTimeSeconds>(unix_time_ms);
}

int64_t NtpTimeUQ32x32ToUnixTimeMs(uint64_t ntp_time_uq32x32) {
  constexpr int64_t kNtpEpochInUnixTimeSeconds = -2208988800ULL;

  return ConvertFixedPointTime<uint64_t, int64_t, NtpTime::kFractionsPerSecond,
                               1000, kNtpEpochInUnixTimeSeconds>(
      ntp_time_uq32x32);
}

int64_t DurationMsToDurationQ32x32(int64_t duration_ms) {
  return ConvertFixedPointTime<int64_t, int64_t, 1000,
                               NtpTime::kFractionsPerSecond, 0>(duration_ms);
}

int64_t DurationQ32x32ToDurationMs(int64_t duration_q32x32) {
  return ConvertFixedPointTime<int64_t, int64_t, NtpTime::kFractionsPerSecond,
                               1000, 0>(duration_q32x32);
}

}  // namespace webrtc
