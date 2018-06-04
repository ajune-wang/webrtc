/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/field_trial_parser.h"

#include <algorithm>
#include <map>

#include "rtc_base/logging.h"

namespace webrtc {
namespace {

int FindOrEnd(std::string str, size_t start, char delimiter) {
  size_t pos = str.find(delimiter, start);
  pos = (pos == std::string::npos) ? str.length() : pos;
  return pos;
}
}  // namespace

FieldTrialParameterInterface::FieldTrialParameterInterface(std::string key)
    : key_(key) {}
FieldTrialParameterInterface::~FieldTrialParameterInterface() = default;
std::string FieldTrialParameterInterface::Key() const {
  return key_;
}

void ParseFieldTrial(
    std::initializer_list<FieldTrialParameterInterface*> fields,
    std::string trial_string) {
  std::map<std::string, FieldTrialParameterInterface*> field_map;
  for (FieldTrialParameterInterface* field : fields) {
    field_map[field->Key()] = field;
  }
  size_t i = 0;
  while (i < trial_string.length()) {
    int val_end = FindOrEnd(trial_string, i, ',');
    int colon_pos = FindOrEnd(trial_string, i, ':');
    int key_end = std::min(val_end, colon_pos);
    int val_begin = key_end + 1;
    std::string key = trial_string.substr(i, key_end - i);
    std::string value = "";
    if (val_end > val_begin)
      value = trial_string.substr(val_begin, val_end - val_begin);
    i = val_end + 1;
    auto field = field_map.find(key);
    if (field != field_map.end()) {
      if (!field->second->TryParse(value)) {
        RTC_LOG(LS_WARNING)
            << "Failed to read field with key: '" << key << "' from \"" << value
            << "\" in trial: \"" << trial_string << "\"";
      }
    } else {
      RTC_LOG(LS_INFO) << "No field with key: '" << key
                       << "' (found in trial: \"" << trial_string << "\")";
    }
  }
}

template <>
rtc::Optional<bool> ParseTypedParameter<bool>(const std::string& str) {
  int int_val;
  // If empty, intepret existance of key as setting it to true.
  if (str.size() == 0 || str == "true") {
    return true;
  } else if (str == "false") {
    return false;
  } else if (sscanf(str.c_str(), "%i", &int_val) == 1) {
    return int_val != 0;
  }
  return rtc::nullopt;
}

template <>
rtc::Optional<double> ParseTypedParameter<double>(const std::string& str) {
  double value;
  if (sscanf(str.c_str(), "%lf", &value) == 1) {
    return value;
  } else {
    return rtc::nullopt;
  }
}

template <>
rtc::Optional<int> ParseTypedParameter<int>(const std::string& str) {
  int value;
  if (sscanf(str.c_str(), "%i", &value) == 1) {
    return value;
  } else {
    return rtc::nullopt;
  }
}

template <>
rtc::Optional<std::string> ParseTypedParameter<std::string>(
    const std::string& str) {
  return str;
}

template class FieldTrialParameter<bool>;
template class FieldTrialParameter<double>;
template class FieldTrialParameter<int>;
template class FieldTrialParameter<std::string>;

template class FieldTrialParameter<rtc::Optional<double>>;
template class FieldTrialParameter<rtc::Optional<int>>;
template class FieldTrialParameter<rtc::Optional<bool>>;
}  // namespace webrtc
