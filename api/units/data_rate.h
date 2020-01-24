/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_UNITS_DATA_RATE_H_
#define API_UNITS_DATA_RATE_H_

#ifdef UNIT_TEST
#include <ostream>  // no-presubmit-check TODO(webrtc:8982)
#endif              // UNIT_TEST

#include <limits>
#include <string>
#include <type_traits>

#include "api/units/data_size.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/units/unit_base.h"

namespace webrtc {
// DataRate is a class that represents a given data rate. This can be used to
// represent bandwidth, encoding bitrate, etc. The internal storage is bits per
// second (bps).
class DataRate final : public rtc_units_impl::RelativeUnit<DataRate> {
 public:
  DataRate() = delete;
  static constexpr DataRate Infinity() { return PlusInfinity(); }
  template <typename T>
  static constexpr DataRate BitsPerSecond(T bits_per_second) {
    static_assert(std::is_arithmetic<T>::value, "");
    return FromValue(bits_per_second);
  }
  template <typename T>
  static constexpr DataRate BytesPerSecond(T bytes_per_second) {
    static_assert(std::is_arithmetic<T>::value, "");
    return FromFraction(8, bytes_per_second);
  }
  template <typename T>
  static constexpr DataRate KilobitsPerSecond(T kilobits_per_sec) {
    static_assert(std::is_arithmetic<T>::value, "");
    return FromFraction(1000, kilobits_per_sec);
  }
  template <typename T = int64_t>
  constexpr T BitsPerSecond() const {
    return ToValue<T>();
  }
  template <typename T = int64_t>
  constexpr T BytesPerSecond() const {
    return ToFraction<8, T>();
  }
  template <typename T = int64_t>
  T KilobitsPerSecond() const {
    return ToFraction<1000, T>();
  }
  constexpr int64_t BitsPerSecondOr(int64_t fallback_value) const {
    return ToValueOr(fallback_value);
  }
  constexpr int64_t KilobitsPerSecondOr(int64_t fallback_value) const {
    return ToFractionOr<1000>(fallback_value);
  }

 private:
  // Bits per second used internally to simplify debugging by making the value
  // more recognizable.
  friend class rtc_units_impl::UnitBase<DataRate>;
  using RelativeUnit::RelativeUnit;
  static constexpr bool one_sided = true;
};

namespace data_rate_impl {
inline int64_t Microbits(const DataSize& size) {
  constexpr int64_t kMaxBeforeConversion =
      std::numeric_limits<int64_t>::max() / 8000000;
  RTC_DCHECK_LE(size.Bytes(), kMaxBeforeConversion)
      << "size is too large to be expressed in microbits";
  return size.Bytes() * 8000000;
}

inline int64_t MillibytePerSec(const DataRate& size) {
  constexpr int64_t kMaxBeforeConversion =
      std::numeric_limits<int64_t>::max() / (1000 / 8);
  RTC_DCHECK_LE(size.BitsPerSecond(), kMaxBeforeConversion)
      << "rate is too large to be expressed in microbytes per second";
  return size.BitsPerSecond() * (1000 / 8);
}
}  // namespace data_rate_impl

inline DataRate operator/(const DataSize size, const TimeDelta duration) {
  return DataRate::BitsPerSecond(data_rate_impl::Microbits(size) /
                                 duration.Microseconds());
}
inline TimeDelta operator/(const DataSize size, const DataRate rate) {
  return TimeDelta::Microseconds(data_rate_impl::Microbits(size) /
                                 rate.BitsPerSecond());
}
inline DataSize operator*(const DataRate rate, const TimeDelta duration) {
  int64_t microbits = rate.BitsPerSecond() * duration.Microseconds();
  return DataSize::Bytes((microbits + 4000000) / 8000000);
}
inline DataSize operator*(const TimeDelta duration, const DataRate rate) {
  return rate * duration;
}

inline DataSize operator/(const DataRate rate, const Frequency frequency) {
  int64_t millihertz = frequency.Millihertz<int64_t>();
  // Note that the value is truncated here reather than rounded, potentially
  // introducing an error of .5 bytes if rounding were expected.
  return DataSize::Bytes(data_rate_impl::MillibytePerSec(rate) / millihertz);
}
inline Frequency operator/(const DataRate rate, const DataSize size) {
  return Frequency::Millihertz(data_rate_impl::MillibytePerSec(rate) /
                               size.Bytes());
}
inline DataRate operator*(const DataSize size, const Frequency frequency) {
  RTC_DCHECK(frequency.IsZero() ||
             size.Bytes() <= std::numeric_limits<int64_t>::max() / 8 /
                                 frequency.Millihertz<int64_t>());
  int64_t millibits_per_second =
      size.Bytes() * 8 * frequency.Millihertz<int64_t>();
  return DataRate::BitsPerSecond((millibits_per_second + 500) / 1000);
}
inline DataRate operator*(const Frequency frequency, const DataSize size) {
  return size * frequency;
}

std::string ToString(DataRate value);
inline std::string ToLogString(DataRate value) {
  return ToString(value);
}

#ifdef UNIT_TEST
inline std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& stream,         // no-presubmit-check TODO(webrtc:8982)
    DataRate value) {
  return stream << ToString(value);
}
#endif  // UNIT_TEST

}  // namespace webrtc

#endif  // API_UNITS_DATA_RATE_H_
