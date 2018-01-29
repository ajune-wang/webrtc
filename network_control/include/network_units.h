/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_CONTROL_INCLUDE_NETWORK_UNITS_H_
#define NETWORK_CONTROL_INCLUDE_NETWORK_UNITS_H_
#include <stdint.h>
#include <limits>
#include <ostream>
#include "rtc_base/checks.h"

namespace webrtc {
namespace units_internal {
inline int64_t DivideAndRound(int64_t numerator, int64_t denominators) {
  if (numerator >= 0) {
    return (numerator + (denominators / 2)) / denominators;
  } else {
    return (numerator + (denominators / 2)) / denominators - 1;
  }
}
}  // namespace units_internal

class TimeDelta {
 public:
  static const TimeDelta kZero;
  static const TimeDelta kMinusInfinity;
  static const TimeDelta kPlusInfinity;
  TimeDelta() : microseconds_(0) {}
  static TimeDelta Zero() { return kZero; }
  static TimeDelta Infinity() { return kPlusInfinity; }
  static TimeDelta seconds(int64_t seconds) { return TimeDelta::s(seconds); }
  static TimeDelta s(int64_t seconds) {
    return TimeDelta::us(seconds * 1000000);
  }
  static TimeDelta ms(int64_t milli_seconds) {
    return TimeDelta::us(milli_seconds * 1000);
  }
  static TimeDelta us(int64_t micro_seconds) {
    // Infinities only allowed via use of explicit constants.
    RTC_DCHECK(micro_seconds > std::numeric_limits<int64_t>::min());
    RTC_DCHECK(micro_seconds < std::numeric_limits<int64_t>::max());
    return TimeDelta(micro_seconds);
  }
  int64_t s() const { return units_internal::DivideAndRound(us(), 1000000); }
  int64_t ms() const { return units_internal::DivideAndRound(us(), 1000); }
  int64_t us() const {
    RTC_DCHECK(IsFinite());
    return microseconds_;
  }
  TimeDelta Abs() { return TimeDelta::us(std::abs(us())); }
  bool IsZero() const { return microseconds_ == 0; }
  bool IsFinite() const {
    return *this != kMinusInfinity && *this != kPlusInfinity;
  }
  bool IsInfinite() const {
    return *this == kPlusInfinity || *this == kMinusInfinity;
  }
  TimeDelta operator+(const TimeDelta& other) const {
    return TimeDelta::us(us() + other.us());
  }
  TimeDelta operator-(const TimeDelta& other) const {
    return TimeDelta::us(us() - other.us());
  }
  TimeDelta operator*(double scalar) const {
    return TimeDelta::us(us() * scalar);
  }
  TimeDelta operator*(int64_t scalar) const {
    return TimeDelta::us(us() * scalar);
  }
  TimeDelta operator*(int32_t scalar) const {
    return TimeDelta::us(us() * scalar);
  }
  bool operator==(const TimeDelta& other) const {
    return microseconds_ == other.microseconds_;
  }
  bool operator!=(const TimeDelta& other) const {
    return microseconds_ != other.microseconds_;
  }
  bool operator<=(const TimeDelta& other) const {
    return microseconds_ <= other.microseconds_;
  }
  bool operator>=(const TimeDelta& other) const {
    return microseconds_ >= other.microseconds_;
  }
  bool operator>(const TimeDelta& other) const {
    return microseconds_ > other.microseconds_;
  }
  bool operator<(const TimeDelta& other) const {
    return microseconds_ < other.microseconds_;
  }

 private:
  explicit TimeDelta(int64_t us) : microseconds_(us) {}
  int64_t microseconds_;
};

class Timestamp {
 public:
  static const Timestamp kPlusInfinity;
  static const Timestamp kNotInitialized;
  Timestamp() : Timestamp(kNotInitialized) {}
  static Timestamp Infinity() { return kPlusInfinity; }
  static Timestamp s(int64_t seconds) { return Timestamp(seconds * 1000000); }
  static Timestamp ms(int64_t millis) { return Timestamp(millis * 1000); }
  static Timestamp us(int64_t micros) { return Timestamp(micros); }
  int64_t s() const { return units_internal::DivideAndRound(us(), 1000000); }
  int64_t ms() const { return units_internal::DivideAndRound(us(), 1000); }
  int64_t us() const {
    RTC_DCHECK(IsFinite());
    return microseconds_;
  }
  bool IsInfinite() const {
    return microseconds_ == kPlusInfinity.microseconds_;
  }
  bool IsInitialized() const {
    return microseconds_ != kNotInitialized.microseconds_;
  }
  bool IsFinite() const { return !IsInfinite() && IsInitialized(); }
  TimeDelta operator-(const Timestamp& other) const {
    return TimeDelta::us(us() - other.us());
  }
  Timestamp operator-(const TimeDelta& delta) const {
    return Timestamp::us(us() - delta.us());
  }
  Timestamp operator+(const TimeDelta& delta) const {
    return Timestamp::us(us() + delta.us());
  }
  bool operator==(const Timestamp& other) const {
    return microseconds_ == other.microseconds_;
  }
  bool operator!=(const Timestamp& other) const {
    return microseconds_ != other.microseconds_;
  }
  bool operator<=(const Timestamp& other) const { return us() <= other.us(); }
  bool operator>=(const Timestamp& other) const { return us() >= other.us(); }
  bool operator>(const Timestamp& other) const { return us() > other.us(); }
  bool operator<(const Timestamp& other) const { return us() < other.us(); }

 private:
  explicit Timestamp(int64_t us) : microseconds_(us) {}
  int64_t microseconds_;
};

/* DataSize is a class represeting a count of bytes. Note that while it can be
 * initialized by a number of bits, it does not guarantee that the resultion is
 * kept and the internal storage is in bytes. The number of bits will be
 * truncated to fit
 */
class DataSize {
 public:
  static const DataSize kZero;
  static const DataSize kPlusInfinity;
  DataSize() : bytes_(0) {}
  static DataSize Zero() { return kZero; }
  static DataSize Infinity() { return kPlusInfinity; }
  static DataSize bytes(int64_t bytes) { return DataSize(bytes); }
  static DataSize bits(int64_t bits) { return DataSize(bits / 8); }
  int64_t bytes() const {
    RTC_DCHECK(bytes_ != kPlusInfinity.bytes_);
    return bytes_;
  }
  int64_t kilobytes() const {
    return units_internal::DivideAndRound(bytes(), 1000);
  }
  int64_t bits() const { return bytes() * 8; }
  int64_t kilobits() const {
    return units_internal::DivideAndRound(bits(), 1000);
  }
  bool IsZero() const { return bytes_ == 0; }
  bool IsFinite() const { return !IsInfinite(); }
  bool IsInfinite() const { return bytes_ == kPlusInfinity.bytes_; }
  DataSize operator-(const DataSize& other) const {
    return DataSize::bytes(bytes() - other.bytes());
  }
  DataSize operator+(const DataSize& other) const {
    return DataSize::bytes(bytes() + other.bytes());
  }
  DataSize operator*(double scalar) const {
    return DataSize::bytes(bytes() * scalar);
  }
  DataSize operator*(int64_t scalar) const {
    return DataSize::bytes(bytes() * scalar);
  }
  DataSize operator*(int32_t scalar) const {
    return DataSize::bytes(bytes() * scalar);
  }
  DataSize operator/(int64_t scalar) const {
    return DataSize::bytes(bytes() / scalar);
  }
  DataSize& operator-=(const DataSize& other) {
    bytes_ -= other.bytes();
    return *this;
  }
  DataSize& operator+=(const DataSize& other) {
    bytes_ += other.bytes();
    return *this;
  }
  bool operator==(const DataSize& other) const {
    return bytes_ == other.bytes_;
  }
  bool operator!=(const DataSize& other) const {
    return bytes_ != other.bytes_;
  }
  bool operator<=(const DataSize& other) const {
    return bytes_ <= other.bytes_;
  }
  bool operator>=(const DataSize& other) const {
    return bytes_ >= other.bytes_;
  }
  bool operator>(const DataSize& other) const { return bytes_ > other.bytes_; }
  bool operator<(const DataSize& other) const { return bytes_ < other.bytes_; }

 private:
  explicit DataSize(int64_t bytes) : bytes_(bytes) {}
  int64_t bytes_;
};

class DataRate {
 public:
  static const DataRate kZero;
  static const DataRate kPlusInfinity;
  DataRate() : DataRate(0) {}
  static DataRate Zero() { return kZero; }
  static DataRate Infinity() { return kPlusInfinity; }
  static DataRate bytes_per_second(int64_t bytes_per_sec) {
    return DataRate(bytes_per_sec * 8);
  }
  static DataRate bits_per_second(int64_t bits_per_sec) {
    return DataRate(bits_per_sec);
  }
  static DataRate bps(int64_t bits_per_sec) {
    return DataRate::bits_per_second(bits_per_sec);
  }
  static DataRate kbps(int64_t kilobits_per_sec) {
    return DataRate::bits_per_second(kilobits_per_sec * 1000);
  }
  int64_t bits_per_second() const {
    RTC_DCHECK(IsFinite());
    return bits_per_sec_;
  }
  int64_t bytes_per_second() const { return bits_per_second() / 8; }
  int64_t bps() const { return bits_per_second(); }
  int64_t kbps() const { return units_internal::DivideAndRound(bps(), 1000); }
  bool IsZero() const { return bits_per_sec_ == 0; }
  bool IsFinite() const { return !IsInfinite(); }
  bool IsInfinite() const {
    return bits_per_sec_ == kPlusInfinity.bits_per_sec_;
  }
  DataRate operator*(double scalar) const {
    return DataRate::bytes_per_second(bytes_per_second() * scalar);
  }
  DataRate operator*(int64_t scalar) const {
    return DataRate::bytes_per_second(bytes_per_second() * scalar);
  }
  DataRate operator*(int32_t scalar) const {
    return DataRate::bytes_per_second(bytes_per_second() * scalar);
  }
  bool operator==(const DataRate& other) const {
    return bits_per_sec_ == other.bits_per_sec_;
  }
  bool operator!=(const DataRate& other) const {
    return bits_per_sec_ != other.bits_per_sec_;
  }
  bool operator<=(const DataRate& other) const {
    return bits_per_sec_ <= other.bits_per_sec_;
  }
  bool operator>=(const DataRate& other) const {
    return bits_per_sec_ >= other.bits_per_sec_;
  }
  bool operator>(const DataRate& other) const {
    return bits_per_sec_ > other.bits_per_sec_;
  }
  bool operator<(const DataRate& other) const {
    return bits_per_sec_ < other.bits_per_sec_;
  }

 private:
  // Bits per second used internally to simplify debugging by making the value
  // more recognizable.
  explicit DataRate(int64_t bits_per_second) : bits_per_sec_(bits_per_second) {}
  int64_t bits_per_sec_;
};

DataRate operator/(const DataSize& size, const TimeDelta& duration);

TimeDelta operator/(const DataSize& size, const DataRate& rate);

inline DataSize operator*(const double& scalar, const DataSize& size) {
  return size * scalar;
}
inline DataSize operator*(const int64_t& scalar, const DataSize& size) {
  return size * scalar;
}
inline DataSize operator*(const int32_t& scalar, const DataSize& size) {
  return size * scalar;
}

inline DataRate operator*(const double& scalar, const DataRate& rate) {
  return rate * scalar;
}
inline DataRate operator*(const int64_t& scalar, const DataRate& rate) {
  return rate * scalar;
}
inline DataRate operator*(const int32_t& scalar, const DataRate& rate) {
  return rate * scalar;
}

inline TimeDelta operator*(const double& scalar, const TimeDelta& delta) {
  return delta * scalar;
}
inline TimeDelta operator*(const int64_t& scalar, const TimeDelta& delta) {
  return delta * scalar;
}
inline TimeDelta operator*(const int32_t& scalar, const TimeDelta& delta) {
  return delta * scalar;
}

DataSize operator*(const DataRate& rate, const TimeDelta& duration);
DataSize operator*(const TimeDelta& duration, const DataRate& rate);

::std::ostream& operator<<(::std::ostream& os, const DataRate& datarate);
::std::ostream& operator<<(::std::ostream& os, const DataSize& datasize);
::std::ostream& operator<<(::std::ostream& os, const Timestamp& timestamp);
::std::ostream& operator<<(::std::ostream& os, const TimeDelta& delta);

}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_UNITS_H_
