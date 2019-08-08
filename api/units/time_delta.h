/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_UNITS_TIME_DELTA_H_
#define API_UNITS_TIME_DELTA_H_

#include <algorithm>
#include <cstdlib>
#include <string>
#include <type_traits>

#include "absl/time/time.h"
#include "rtc_base/units/unit_base.h"

namespace webrtc {

// TimeDelta represents the difference between two timestamps. Commonly this can
// be a duration. However since two Timestamps are not guaranteed to have the
// same epoch (they might come from different computers, making exact
// synchronisation infeasible), the duration covered by a TimeDelta can be
// undefined. To simplify usage, it can be constructed and converted to
// different units, specifically seconds (s), milliseconds (ms) and
// microseconds (us).
class TimeDelta final : public absl::Duration {
 public:
  using Duration::Duration;

  constexpr explicit TimeDelta(absl::Duration duration) : Duration(duration) {}
  TimeDelta& operator=(absl::Duration duration) {
    static_cast<absl::Duration&>(*this) = duration;
    return *this;
  }

  template <int64_t seconds>
  static constexpr TimeDelta Seconds() {
    return TimeDelta(absl::Seconds(seconds));
  }
  template <int64_t ms>
  static constexpr TimeDelta Millis() {
    return TimeDelta(absl::Milliseconds(ms));
  }
  template <int64_t us>
  static constexpr TimeDelta Micros() {
    return TimeDelta(absl::Microseconds(us));
  }
  template <typename T>
  static constexpr TimeDelta seconds(T seconds) {
    return TimeDelta(absl::Seconds(seconds));
  }
  template <typename T>
  static constexpr TimeDelta ms(T milliseconds) {
    return TimeDelta(absl::Milliseconds(milliseconds));
  }
  template <typename T>
  static constexpr TimeDelta us(T microseconds) {
    return TimeDelta(absl::Microseconds(microseconds));
  }
  template <typename T = int64_t>
  T seconds() const {
    return absl::ToInt64Seconds(*this);
  }
  template <typename T = int64_t>
  T ms() const {
    return absl::ToInt64Milliseconds(*this);
  }
  template <typename T = int64_t>
  T us() const {
    return absl::ToInt64Microseconds(*this);
  }
  template <typename T = int64_t>
  T ns() const {
    return absl::ToInt64Nanoseconds(*this);
  }

  int64_t seconds_or(int64_t fallback_value) const {
    return IsFinite() ? seconds() : fallback_value;
  }
  int64_t ms_or(int64_t fallback_value) const {
    return IsFinite() ? ms() : fallback_value;
  }
  int64_t us_or(int64_t fallback_value) const {
    return IsFinite() ? us() : fallback_value;
  }

  TimeDelta Abs() const { return TimeDelta(absl::AbsDuration(*this)); }

  static constexpr TimeDelta Zero() { return TimeDelta(absl::ZeroDuration()); }
  static constexpr TimeDelta PlusInfinity() {
    return TimeDelta(absl::InfiniteDuration());
  }
  static constexpr TimeDelta MinusInfinity() {
    return TimeDelta(-absl::InfiniteDuration());
  }
  constexpr bool IsZero() const { return *this == absl::ZeroDuration(); }
  constexpr bool IsPlusInfinity() const {
    return *this == absl::InfiniteDuration();
  }
  constexpr bool IsMinusInfinity() const {
    return *this == -absl::InfiniteDuration();
  }
  constexpr bool IsInfinite() const {
    return IsPlusInfinity() || IsMinusInfinity();
  }
  constexpr bool IsFinite() const { return !IsInfinite(); }

  TimeDelta Clamped(absl::Duration min_value, absl::Duration max_value) const {
    return TimeDelta(std::max(
        min_value, std::min(static_cast<absl::Duration>(*this), max_value)));
  }
  void Clamp(absl::Duration min_value, absl::Duration max_value) {
    *this = Clamped(min_value, max_value);
  }

  friend TimeDelta operator+(TimeDelta lhs, TimeDelta rhs) {
    return TimeDelta(static_cast<absl::Duration>(lhs) +
                     static_cast<absl::Duration>(rhs));
  }
};

inline std::string ToString(TimeDelta value) {
  return absl::FormatDuration(value);
}
inline std::string ToLogString(TimeDelta value) {
  return absl::FormatDuration(value);
}

}  // namespace webrtc

namespace absl {

inline std::string ToLogString(Duration value) {
  return FormatDuration(value);
}

}  // namespace absl

#endif  // API_UNITS_TIME_DELTA_H_
