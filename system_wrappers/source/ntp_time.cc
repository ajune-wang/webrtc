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

// Calculates "input / kDivisor" and returns the floored integer part in
// |output_floor| and the non-negative modulos in |output_modulos|.
template <typename ValueType, ValueType kDivisor>
void DivideAndGetFloorAndModulos(ValueType input,
                                 ValueType* output_floor,
                                 ValueType* output_modulos) {
  RTC_DCHECK(output_floor != nullptr);
  RTC_DCHECK(output_modulos != nullptr);
  static_assert(kDivisor > 0, "Divisor must be positive.");

  *output_floor = input / kDivisor;
  *output_modulos = input % kDivisor;

  if (*output_modulos < 0) {
    *output_floor -= 1;
    *output_modulos += kDivisor;
  }
}

// Calculates "value / kDivisor" and rounds half towards positive infinity.
template <typename ValueType, ValueType kDivisor>
ValueType DivideAndRound(ValueType value) {
  RTC_DCHECK_GE(value, 0);
  static_assert(kDivisor > 0, "Divisor must be positive.");
  static_assert(kDivisor % 2 == 0, "Divisor must be even.");

  ValueType quotient = value / kDivisor;
  ValueType remainder = value % kDivisor;

  return quotient + (remainder >= (kDivisor / 2));
}

// Calculates "(input / kInputSecondsDivisor + kOutputSecondsOffset) *
// kOutputSecondsMultiplier" ands rounds half towards positive infinity.
template <typename InputType,
          typename OutputType,
          InputType kInputSecondsDivisor,
          OutputType kOutputSecondsMultiplier,
          OutputType kOutputSecondsOffset>
OutputType ConvertFixedPointTime(InputType input) {
  static_assert(kInputSecondsDivisor > 0, "Divisor must be positive.");
  static_assert(kOutputSecondsMultiplier > 0, "Multiplier must be positive.");

  InputType input_seconds;
  InputType input_fractional;
  DivideAndGetFloorAndModulos<InputType, kInputSecondsDivisor>(
      input, &input_seconds, &input_fractional);

  OutputType output_seconds = input_seconds + kOutputSecondsOffset;
  OutputType output_fractional =
      DivideAndRound<OutputType, kInputSecondsDivisor>(
          input_fractional * kOutputSecondsMultiplier);

  return (output_seconds * kOutputSecondsMultiplier) + output_fractional;
}

}  // namespace

uint64_t UnixTimeMsToNtpTimeUQ32x32(int64_t unix_time_ms) {
  constexpr uint64_t kUnixEpochInNtpTimeSeconds = 2208988800;

  return ConvertFixedPointTime<int64_t, uint64_t, 1000,
                               NtpTime::kFractionsPerSecond,
                               kUnixEpochInNtpTimeSeconds>(unix_time_ms);
}

int64_t NtpTimeUQ32x32ToUnixTimeMs(uint64_t ntp_time_uq32x32) {
  constexpr int64_t kNtpEpochInUnixTimeSeconds = -int64_t{2208988800};

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
