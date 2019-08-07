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

template <typename InputType, typename OutputType>
OutputType ConvertFixedPointTime(InputType input,
                                 InputType input_seconds_divisor,
                                 OutputType output_seconds_multiplier,
                                 OutputType output_seconds_offset) {
  InputType input_seconds = input / input_seconds_divisor;
  InputType input_fractional = input % input_seconds_divisor;

  if (input_fractional < 0) {
    input_seconds -= 1;
    input_fractional += input_seconds_divisor;
  }

  OutputType output_seconds = static_cast<uint64_t>(input_seconds) +
                              static_cast<uint64_t>(output_seconds_offset);

  OutputType output_fractional;
  {
    uint64_t numerator = static_cast<uint64_t>(input_fractional) *
                         static_cast<uint64_t>(output_seconds_multiplier);
    uint64_t denominator = static_cast<uint64_t>(input_seconds_divisor);

    uint64_t quotient = numerator / denominator;
    uint64_t remainder = numerator % denominator;

    if (remainder >= ((denominator + 1) / 2)) {
      output_fractional = quotient + 1;
    } else {
      output_fractional = quotient;
    }
  }

  return (output_seconds * output_seconds_multiplier) + output_fractional;
}

}  // namespace

uint64_t UnixTimeMsToNtpTimeUQ32x32(int64_t unix_time_ms) {
  return ConvertFixedPointTime<int64_t, uint64_t>(
      unix_time_ms, 1000, NtpTime::kFractionsPerSecond, 2208988800LL);
}

int64_t NtpTimeUQ32x32ToUnixTimeMs(uint64_t ntp_time_uq32x32) {
  return ConvertFixedPointTime<uint64_t, int64_t>(
      ntp_time_uq32x32, NtpTime::kFractionsPerSecond, 1000, -2208988800LL);
}

int64_t DurationMsToDurationQ32x32(int64_t duration_ms) {
  return ConvertFixedPointTime<int64_t, int64_t>(
      duration_ms, 1000, NtpTime::kFractionsPerSecond, 0);
}

int64_t DurationQ32x32ToDurationMs(int64_t duration_q32x32) {
  return ConvertFixedPointTime<int64_t, int64_t>(
      duration_q32x32, NtpTime::kFractionsPerSecond, 1000, 0);
}

}  // namespace webrtc
