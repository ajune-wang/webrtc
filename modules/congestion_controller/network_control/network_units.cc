/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/network_control/include/network_units.h"
#include <cmath>

namespace webrtc {
namespace {
int64_t kPlusInfinityVal = std::numeric_limits<int64_t>::max();
int64_t kMinusInfinityVal = std::numeric_limits<int64_t>::min();
int64_t kSignedNotInitializedVal = kMinusInfinityVal + 1;
int64_t kNotInitializedVal = -1;
}  // namespace
const TimeDelta TimeDelta::kZero = TimeDelta(0);
const TimeDelta TimeDelta::kMinusInfinity = TimeDelta(kMinusInfinityVal);
const TimeDelta TimeDelta::kPlusInfinity = TimeDelta(kPlusInfinityVal);
const TimeDelta TimeDelta::kNotInitialized =
    TimeDelta(kSignedNotInitializedVal);

const Timestamp Timestamp::kPlusInfinity = Timestamp(kPlusInfinityVal);
const Timestamp Timestamp::kNotInitialized = Timestamp(kNotInitializedVal);

const DataRate DataRate::kZero = DataRate(0);
const DataRate DataRate::kPlusInfinity = DataRate(kPlusInfinityVal);
const DataRate DataRate::kNotInitialized = DataRate(kNotInitializedVal);

const DataSize DataSize::kZero = DataSize(0);
const DataSize DataSize::kPlusInfinity = DataSize(kPlusInfinityVal);
const DataSize DataSize::kNotInitialized = DataSize(kNotInitializedVal);

TimeDelta TimeDelta::operator*(double scalar) const {
  return TimeDelta::us(std::round(us() * scalar));
}

DataSize DataSize::operator*(double scalar) const {
  return DataSize::bytes(std::round(bytes() * scalar));
}

DataRate DataRate::operator*(double scalar) const {
  return DataRate::bytes_per_second(std::round(bytes_per_second() * scalar));
}

DataRate operator/(const DataSize& size, const TimeDelta& duration) {
  RTC_DCHECK(size.bytes() < std::numeric_limits<int64_t>::max() / 1000000)
      << "size is too large, size: " << size.bytes() << " is not less than "
      << std::numeric_limits<int64_t>::max() / 1000000;
  auto bytes_per_sec = size.bytes() * 1000000 / duration.us();
  return DataRate::bytes_per_second(bytes_per_sec);
}

TimeDelta operator/(const DataSize& size, const DataRate& rate) {
  RTC_DCHECK(size.bytes() < std::numeric_limits<int64_t>::max() / 1000000)
      << "size is too large, size: " << size.bytes() << " is not less than "
      << std::numeric_limits<int64_t>::max() / 1000000;
  auto microseconds = size.bytes() * 1000000 / rate.bytes_per_second();
  return TimeDelta::us(microseconds);
}

DataSize operator*(const DataRate& rate, const TimeDelta& duration) {
  auto micro_bytes = rate.bytes_per_second() * duration.us();
  auto bytes = units_internal::DivideAndRound(micro_bytes, 1000000);
  return DataSize::bytes(bytes);
}

DataSize operator*(const TimeDelta& duration, const DataRate& rate) {
  return rate * duration;
}

::std::string DataRate::ToString() const {
  if (*this == DataRate::kPlusInfinity) {
    return "inf bps";
  } else if (*this == DataRate::kNotInitialized) {
    return "? bps";
  } else {
    return std::to_string(bps()) + " bps";
  }
}
::std::string DataSize::ToString() const {
  if (*this == DataSize::kPlusInfinity) {
    return "inf bytes";
  } else if (*this == DataSize::kNotInitialized) {
    return "? bytes";
  } else {
    return std::to_string(bytes()) + " bytes";
  }
}
::std::string Timestamp::ToString() const {
  if (*this == Timestamp::kPlusInfinity) {
    return "inf ms";
  } else if (*this == Timestamp::kNotInitialized) {
    return "? ms";
  } else {
    return std::to_string(ms()) + " ms";
  }
}
::std::string TimeDelta::ToString() const {
  if (*this == TimeDelta::kPlusInfinity) {
    return "+inf ms";
  } else if (*this == TimeDelta::kMinusInfinity) {
    return "-inf ms";
  } else if (*this == TimeDelta::kNotInitialized) {
    return "? ms";
  } else {
    return std::to_string(ms()) + " ms";
  }
}

}  // namespace webrtc
