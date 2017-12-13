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

namespace webrtc {
namespace network {
namespace units {
namespace internal {
constexpr int64_t kMilli = 1000;
constexpr int64_t kMicro = 1000000;
constexpr int64_t kBitsPerByte = 8;
constexpr int64_t kKilo = 1000;
}  // namespace internal

class TimeDelta {
 public:
  static const TimeDelta kZero;
  static const TimeDelta kMinusInfinity;
  static const TimeDelta kPlusInfinity;

  TimeDelta() : microseconds_(0) {}
  static TimeDelta s(int64_t seconds) {
    return TimeDelta(seconds * internal::kMicro);
  }
  static TimeDelta ms(int64_t millis) {
    return TimeDelta(millis * internal::kMilli);
  }
  static TimeDelta us(int64_t micros) { return TimeDelta(micros); }

  int64_t s() const { return (us() + internal::kMicro / 2) / internal::kMicro; }
  int64_t ms() const {
    return (us() + internal::kMilli / 2) / internal::kMilli;
  }
  int64_t us() const { return microseconds_; }
  TimeDelta operator-(const TimeDelta& other) const {
    return us(us() - other.us());
  }
  TimeDelta operator+(const TimeDelta& other) const {
    return us(us() + other.us());
  }
  bool IsFinite() const {
    return *this != kMinusInfinity && *this != kPlusInfinity;
  }

  bool operator==(const int& other) const { return other == 0 && us() == 0; }

  bool operator==(const TimeDelta& other) const { return us() == other.us(); }
  bool operator!=(const TimeDelta& other) const { return us() != other.us(); }
  bool operator<=(const TimeDelta& other) const { return us() <= other.us(); }
  bool operator>=(const TimeDelta& other) const { return us() >= other.us(); }
  bool operator>(const TimeDelta& other) const { return us() > other.us(); }
  bool operator<(const TimeDelta& other) const { return us() < other.us(); }

 private:
  explicit TimeDelta(int64_t us) : microseconds_(us) {}
  int64_t microseconds_;
};

class Timestamp {
 public:
  Timestamp() : microseconds_(0) {}
  static Timestamp s(int64_t seconds) {
    return Timestamp(seconds * internal::kMicro);
  }
  static Timestamp ms(int64_t millis) {
    return Timestamp(millis * internal::kMilli);
  }
  static Timestamp us(int64_t micros) { return Timestamp(micros); }

  int64_t s() const { return (us() + internal::kMicro / 2) / internal::kMicro; }
  int64_t ms() const {
    return (us() + internal::kMilli / 2) / internal::kMilli;
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
  DataSize() : bytes_(0) {}

  static DataSize bytes(int64_t bytes) { return DataSize(bytes); }
  static DataSize bits(int64_t bits) {
    return DataSize(bits / internal::kBitsPerByte);
  }

  int64_t bytes() const { return bytes_; }
  int64_t kilobytes() const {
    return (bytes() + internal::kKilo / 2) / internal::kKilo;
  }

  int64_t bits() const { return bytes() * internal::kBitsPerByte; }
  int64_t kilobits() const {
    return (bits() + internal::kKilo / 2) / internal::kKilo;
  }

  DataSize operator-(const DataSize& other) const {
    return bytes(bytes() - other.bytes());
  }
  DataSize operator+(const DataSize& other) const {
    return bytes(bytes() + other.bytes());
  }
  DataSize operator*(double scalar) const { return bytes(bytes() * scalar); }
  DataSize operator*(int64_t scalar) const { return bytes(bytes() * scalar); }

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
  static const DataRate kMinusInfinity;
  static const DataRate kPlusInfinity;

  DataRate() : bytes_per_sec_(0) {}

  static DataRate per_second(const DataSize& data_per_sec) {
    return DataRate(data_per_sec.bytes());
  }
  static DataRate bytes_per_second(int64_t bytes_per_sec) {
    return DataRate(bytes_per_sec);
  }
  static DataRate bits_per_second(int64_t bits_per_sec) {
    return DataRate(bits_per_sec / internal::kBitsPerByte);
  }
  static DataRate bps(int64_t bits_per_sec) {
    return DataRate::bits_per_second(bits_per_sec);
  }
  static DataRate kbps(int64_t kilobits_per_sec) {
    return DataRate::bits_per_second(kilobits_per_sec * internal::kKilo);
  }

  int64_t bytes_per_second() const { return bytes_per_sec_; }
  int64_t bits_per_second() const {
    return bytes_per_sec_ * internal::kBitsPerByte;
  }
  int64_t bps() const { return bits_per_second(); }
  int64_t kbps() const {
    return (bps() + internal::kKilo / 2) / internal::kKilo;
  }

  DataRate operator*(double scalar) const {
    return bytes_per_second(bytes_per_second() * scalar);
  }

  DataRate operator*(int64_t scalar) const {
    return bytes_per_second(bytes_per_second() * scalar);
  }

  bool operator==(const int& other) const {
    return other == 0 && bytes_per_second() == 0;
  }
  bool operator==(const DataRate& other) const {
    return bytes_per_second() == other.bytes_per_second();
  }
  bool operator!=(const DataRate& other) const {
    return bytes_per_second() != other.bytes_per_second();
  }
  bool operator<=(const DataRate& other) const {
    return bytes_per_second() <= other.bytes_per_second();
  }
  bool operator>=(const DataRate& other) const {
    return bytes_per_second() >= other.bytes_per_second();
  }
  bool operator>(const DataRate& other) const {
    return bytes_per_second() > other.bytes_per_second();
  }
  bool operator<(const DataRate& other) const {
    return bytes_per_second() < other.bytes_per_second();
  }

 private:
  explicit DataRate(int64_t bytes_per_second)
      : bytes_per_sec_(bytes_per_second) {}
  int64_t bytes_per_sec_;
};

DataRate operator/(const DataSize& size, const TimeDelta& duration);
DataSize operator*(const DataRate& rate, const TimeDelta& duration);
DataSize operator*(const TimeDelta& duration, const DataRate& rate);

::std::ostream& operator<<(::std::ostream& os, const DataRate& datarate);
::std::ostream& operator<<(::std::ostream& os, const DataSize& datasize);
::std::ostream& operator<<(::std::ostream& os, const Timestamp& timestamp);
::std::ostream& operator<<(::std::ostream& os, const TimeDelta& delta);

}  // namespace units
}  // namespace network
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_UNITS_H_
