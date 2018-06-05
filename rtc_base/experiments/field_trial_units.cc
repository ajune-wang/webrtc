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

template <>
rtc::Optional<DataRate> ParseTypedParameter<DataRate>(const std::string& str) {
  double double_val;
  char unit_char[6];
  unit_char[0] = 0;
  if (sscanf(str.c_str(), "%lf%5s", &double_val, unit_char) >= 1) {
    std::string unit = unit_char;
    if (unit.empty() || unit == "kbps") {
      return DataRate::kbps(double_val);
    } else if (unit == "bps") {
      return DataRate::bps(double_val);
    }
  }
  return rtc::nullopt;
}

template <>
rtc::Optional<DataSize> ParseTypedParameter<DataSize>(const std::string& str) {
  double double_val;
  char unit_char[7];
  unit_char[0] = 0;
  if (sscanf(str.c_str(), "%lf%6s", &double_val, unit_char) >= 1) {
    std::string unit = unit_char;
    if (unit.empty() || unit == "bytes")
      return DataSize::bytes(double_val);
  }
  return rtc::nullopt;
}

template <>
rtc::Optional<TimeDelta> ParseTypedParameter<TimeDelta>(
    const std::string& str) {
  double double_val;
  char unit_char[9];
  unit_char[0] = 0;
  if (sscanf(str.c_str(), "%lf%8s", &double_val, unit_char) >= 1) {
    std::string unit = unit_char;
    if (unit == "s" || unit == "seconds") {
      return TimeDelta::seconds(double_val);
    } else if (unit == "us") {
      return TimeDelta::us(double_val);
    } else if (unit.empty() || unit == "ms") {
      return TimeDelta::ms(double_val);
    }
  }
  return rtc::nullopt;
}

template class FieldTrialParameter<DataRate>;
template class FieldTrialParameter<DataSize>;
template class FieldTrialParameter<TimeDelta>;

template class FieldTrialParameter<rtc::Optional<DataRate>>;
template class FieldTrialParameter<rtc::Optional<DataSize>>;
template class FieldTrialParameter<rtc::Optional<TimeDelta>>;
}  // namespace webrtc
