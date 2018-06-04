/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/field_trial_units.h"

#include <string>

namespace webrtc {
namespace {
bool ParseDoubleParameterAndUnit(const std::string& str,
                                 std::string* unit,
                                 double* value) {
  constexpr size_t kMaxStringPlusNullLength = 128;
  if (str.size() >= kMaxStringPlusNullLength)
    return false;
  char unit_char[kMaxStringPlusNullLength];
  size_t parts_read = sscanf(str.c_str(), "%lf%s", value, unit_char);
  if (parts_read == 2)
    *unit = unit_char;
  return parts_read >= 1;
}
}  // namespace

template <>
struct TypedParameterParser<DataRate> {
  explicit TypedParameterParser(const std::string& str) {
    if (ParseDoubleParameterAndUnit(str, &unit, &double_val))
      valid = unit.empty() || unit == "kbps" || unit == "bps";
  }
  DataRate Get() const {
    if (unit == "bps") {
      return DataRate::bps(double_val);
    }
    return DataRate::kbps(double_val);
  }
  bool valid = false;
  double double_val;
  std::string unit;
};

template <>
struct TypedParameterParser<DataSize> {
  explicit TypedParameterParser(const std::string& str) {
    if (ParseDoubleParameterAndUnit(str, &unit, &double_val))
      valid = unit.empty() || unit == "bytes";
  }
  DataSize Get() const { return DataSize::bytes(double_val); }
  bool valid = false;
  double double_val;
  std::string unit;
};

template <>
struct TypedParameterParser<TimeDelta> {
  explicit TypedParameterParser(const std::string& str) {
    if (ParseDoubleParameterAndUnit(str, &unit, &double_val))
      valid = unit.empty() || unit == "kbps" || unit == "ms" || unit == "s" ||
              unit == "seconds";
  }
  TimeDelta Get() const {
    if (unit == "s" || unit == "seconds") {
      return TimeDelta::seconds(double_val);
    } else if (unit == "us") {
      return TimeDelta::us(double_val);
    }
    return TimeDelta::ms(double_val);
  }
  bool valid = false;
  double double_val;
  std::string unit;
};

template class FieldTrialParameter<DataRate>;
template class FieldTrialParameter<DataSize>;
template class FieldTrialParameter<TimeDelta>;

template class FieldTrialParameter<rtc::Optional<DataRate>>;
template class FieldTrialParameter<rtc::Optional<DataSize>>;
template class FieldTrialParameter<rtc::Optional<TimeDelta>>;
}  // namespace webrtc
