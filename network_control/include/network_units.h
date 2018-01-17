/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
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
namespace network_units_constants {
constexpr int64_t kMilli = 1000;
constexpr int64_t kMicro = 1000000;
constexpr int64_t kBitsPerByte = 8;
constexpr int64_t kKilo = 1000;
}  // namespace network_units_constants

class TimeDelta {
 public:
  static const TimeDelta kZero;
  static const TimeDelta kMinusInfinity;
  static const TimeDelta kPlusInfinity;

  TimeDelta() : microseconds_(0) {}

  static TimeDelta Zero() { return kZero; }
  static TimeDelta Infinity() { return kPlusInfinity; }

  static TimeDelta seconds(int64_t seconds) {
    return TimeDelta(seconds * network_units_constants::kMicro);
  }
  static TimeDelta s(int64_t seconds) { return TimeDelta::seconds(seconds); }
  static TimeDelta ms(int64_t millis) {
    return TimeDelta(millis * network_units_constants::kMilli);
  }
  static TimeDelta us(int64_t micros) { return TimeDelta(micros); }

  int64_t s() const {
    return (us() + network_units_constants::kMicro / 2) /
           network_units_constants::kMicro;
  }
  int64_t ms() const {
    return (us() + network_units_constants::kMilli / 2) /
           network_units_constants::kMilli;
  }
  int64_t us() const {
    RTC_DCHECK(IsFinite());
    return microseconds_;
  }

  TimeDelta Abs() { return us(std::abs(us())); }
  bool IsZero() const { return microseconds_ == 0; }
  bool IsFinite() const {
    return *this != kMinusInfinity && *this != kPlusInfinity;
  }
  bool IsInfinite() const {
    return *this == kPlusInfinity || *this == kMinusInfinity;
  }

  TimeDelta operator-(const TimeDelta& other) const {
    return us(us() - other.us());
  }
  TimeDelta operator+(const TimeDelta& other) const {
    return us(us() + other.us());
  }

  TimeDelta operator*(double scalar) const { return us(us() * scalar); }
  TimeDelta operator*(int64_t scalar) const { return us(us() * scalar); }
  TimeDelta operator*(int32_t scalar) const { return us(us() * scalar); }

  bool operator==(const int& other) const { return other == 0 && us() == 0; }

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
  Timestamp() : microseconds_(0) {}

  static Timestamp s(int64_t seconds) {
    return Timestamp(seconds * network_units_constants::kMicro);
  }
  static Timestamp ms(int64_t millis) {
    return Timestamp(millis * network_units_constants::kMilli);
  }
  static Timestamp us(int64_t micros) { return Timestamp(micros); }

  int64_t s() const {
    return (us() + network_units_constants::kMicro / 2) /
           network_units_constants::kMicro;
  }
  int64_t ms() const {
    return (us() + network_units_constants::kMilli / 2) /
           network_units_constants::kMilli;
  }
  int64_t us() const { return microseconds_; }

  TimeDelta operator-(const Timestamp& other) const {
    return TimeDelta::us(us() - other.us());
  }

  Timestamp operator-(const TimeDelta& delta) const {
    return us(us() - delta.us());
  }
  Timestamp operator+(const TimeDelta& delta) const {
    return us(us() + delta.us());
  }

  bool IsInitialized() const { return microseconds_ != 0; }

  bool operator==(const Timestamp& other) const { return us() == other.us(); }
  bool operator!=(const Timestamp& other) const { return us() != other.us(); }
  bool operator<=(const Timestamp& other) const { return us() <= other.us(); }
  bool operator>=(const Timestamp& other) const { return us() >= other.us(); }
  bool operator>(const Timestamp& other) const { return us() > other.us(); }
  bool operator<(const Timestamp& other) const { return us() < other.us(); }

 private:
  explicit Timestamp(int64_t us) : microseconds_(us) {}
  int64_t microseconds_;
};

class DataSize {
 public:
  static const DataSize kZero;
  static const DataSize kPlusInfinity;

  DataSize() : bytes_(0) {}
  static DataSize Zero() { return kZero; }

  static DataSize bytes(int64_t bytes) { return DataSize(bytes); }
  static DataSize bits(int64_t bits) {
    return DataSize(bits / network_units_constants::kBitsPerByte);
  }

  int64_t bytes() const { return bytes_; }
  int64_t kilobytes() const {
    return (bytes() + network_units_constants::kKilo / 2) /
           network_units_constants::kKilo;
  }

  int64_t bits() const {
    return bytes() * network_units_constants::kBitsPerByte;
  }
  int64_t kilobits() const {
    return (bits() + network_units_constants::kKilo / 2) /
           network_units_constants::kKilo;
  }

  DataSize operator-(const DataSize& other) const {
    return bytes(bytes() - other.bytes());
  }
  DataSize operator+(const DataSize& other) const {
    return bytes(bytes() + other.bytes());
  }
  DataSize operator*(double scalar) const { return bytes(bytes() * scalar); }
  DataSize operator*(int64_t scalar) const { return bytes(bytes() * scalar); }
  DataSize operator*(int32_t scalar) const { return bytes(bytes() * scalar); }

  DataSize operator/(int64_t scalar) const { return bytes(bytes() / scalar); }

  DataSize& operator-=(const DataSize& other) {
    bytes_ -= other.bytes();
    return *this;
  }

  DataSize& operator+=(const DataSize& other) {
    bytes_ += other.bytes();
    return *this;
  }

  bool operator==(const int& other) const { return other == 0 && bytes() == 0; }

  bool operator==(const DataSize& other) const {
    return bytes() == other.bytes();
  }
  bool operator!=(const DataSize& other) const {
    return bytes() != other.bytes();
  }
  bool operator<=(const DataSize& other) const {
    return bytes() <= other.bytes();
  }
  bool operator>=(const DataSize& other) const {
    return bytes() >= other.bytes();
  }
  bool operator>(const DataSize& other) const {
    return bytes() > other.bytes();
  }
  bool operator<(const DataSize& other) const {
    return bytes() < other.bytes();
  }

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

  static DataRate per_second(const DataSize& data_per_sec) {
    return DataRate(data_per_sec.bits());
  }
  static DataRate bytes_per_second(int64_t bytes_per_sec) {
    return DataRate(bytes_per_sec * network_units_constants::kBitsPerByte);
  }
  static DataRate bits_per_second(int64_t bits_per_sec) {
    return DataRate(bits_per_sec);
  }
  static DataRate bps(int64_t bits_per_sec) {
    return DataRate::bits_per_second(bits_per_sec);
  }
  static DataRate kbps(int64_t kilobits_per_sec) {
    return DataRate::bits_per_second(kilobits_per_sec *
                                     network_units_constants::kKilo);
  }
  int64_t bits_per_second() const {
    RTC_DCHECK(IsFinite());
    return bits_per_sec_;
  }

  int64_t bytes_per_second() const {
    return bits_per_second() / network_units_constants::kBitsPerByte;
  }

  int64_t bps() const { return bits_per_second(); }
  int64_t kbps() const {
    return (bps() + network_units_constants::kKilo / 2) /
           network_units_constants::kKilo;
  }

  bool IsZero() const { return bits_per_sec_ == 0; }
  bool IsFinite() const { return *this != kPlusInfinity; }
  bool IsInfinity() const { return *this == kPlusInfinity; }

  DataRate operator*(double scalar) const {
    return bytes_per_second(bytes_per_second() * scalar);
  }
  DataRate operator*(int64_t scalar) const {
    return bytes_per_second(bytes_per_second() * scalar);
  }
  DataRate operator*(int32_t scalar) const {
    return bytes_per_second(bytes_per_second() * scalar);
  }

  bool operator==(const int& other) const {
    return other == 0 && bytes_per_second() == 0;
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
  // Bits per second used network_units_constantsly to simplify debugging by
  // making the value more recognizable
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
