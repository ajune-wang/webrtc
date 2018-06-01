/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/field_trial_parameters.h"

namespace webrtc {

template <>
struct TypedParameterParser<bool> {
  explicit TypedParameterParser(const std::string& str) {
    int int_val;
    if (str.size() == 0) {
      value = true;
      valid = true;
    } else if (sscanf(str.c_str(), "%i", &int_val) == 1) {
      value = int_val != 0;
      valid = true;
    }
  }
  bool Get() const { return value; }
  bool valid = false;
  bool value;
};

template <>
struct TypedParameterParser<double> {
  explicit TypedParameterParser(const std::string& str) {
    valid = sscanf(str.c_str(), "%lf", &value) == 1;
  }
  double Get() const { return value; }
  bool valid = false;
  double value;
};

template <>
struct TypedParameterParser<int64_t> {
  explicit TypedParameterParser(const std::string& str) {
    valid = sscanf(str.c_str(), "%li", &value) == 1;
  }
  int64_t Get() const { return value; }
  bool valid = false;
  int64_t value;
};

template <>
struct TypedParameterParser<std::string> {
  explicit TypedParameterParser(const std::string& str) : value(str) {}
  std::string Get() const { return value; }
  bool valid = true;
  const std::string& value;
};

template class FieldTrialParameter<bool>;
template class FieldTrialParameter<double>;
template class FieldTrialParameter<int64_t>;
template class FieldTrialParameter<std::string>;

template class FieldTrialParameter<rtc::Optional<double>>;
template class FieldTrialParameter<rtc::Optional<int64_t>>;
template class FieldTrialParameter<rtc::Optional<bool>>;
}  // namespace webrtc
