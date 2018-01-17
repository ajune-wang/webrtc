/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "network_control/include/network_units.h"

namespace webrtc {

const TimeDelta TimeDelta::kZero = TimeDelta(0);
const TimeDelta TimeDelta::kMinusInfinity =
    TimeDelta(std::numeric_limits<int64_t>::min());
const TimeDelta TimeDelta::kPlusInfinity =
    TimeDelta(std::numeric_limits<int64_t>::max());

const DataRate DataRate::kZero = DataRate(0);
const DataRate DataRate::kPlusInfinity =
    DataRate(std::numeric_limits<int64_t>::max());

DataRate operator/(const DataSize& size, const TimeDelta& duration) {
  auto bytes_per_sec =
      size.bytes() * network_units_constants::kMicro / duration.us();
  return DataRate::bytes_per_second(bytes_per_sec);
}

TimeDelta operator/(const DataSize& size, const DataRate& rate) {
  auto microseconds =
      size.bytes() * network_units_constants::kMicro / rate.bytes_per_second();
  return TimeDelta::us(microseconds);
}

DataSize operator*(const DataRate& rate, const TimeDelta& duration) {
  auto micro_bytes = rate.bytes_per_second() * duration.us();
  auto bytes = (micro_bytes + network_units_constants::kMicro / 2) /
               network_units_constants::kMicro;
  return DataSize::bytes(bytes);
}

const DataSize DataSize::kZero = DataSize(0);
const DataSize DataSize::kPlusInfinity =
    DataSize(std::numeric_limits<int64_t>::max());

DataSize operator*(const TimeDelta& duration, const DataRate& rate) {
  return rate * duration;
}

::std::ostream& operator<<(::std::ostream& os, const DataRate& datarate) {
  return os << datarate.bps() << " bps";
}
::std::ostream& operator<<(::std::ostream& os, const DataSize& datasize) {
  return os << datasize.bytes() << " bytes";
}
::std::ostream& operator<<(::std::ostream& os, const Timestamp& timestamp) {
  return os << timestamp.ms() << " ms";
}
::std::ostream& operator<<(::std::ostream& os, const TimeDelta& delta) {
  if (delta == TimeDelta::kPlusInfinity)
    return os << "+∞ ms";
  else if (delta == TimeDelta::kMinusInfinity)
    return os << delta.ms() << "-∞ ms";

  else
    return os << delta.ms() << " ms";
}
}  // namespace webrtc
