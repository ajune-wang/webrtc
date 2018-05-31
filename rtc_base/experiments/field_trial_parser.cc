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

template <typename T>
bool TryParseType(const std::string& str, T* value) {
  static_assert(sizeof(T) == 0, "TryParse not implemented for this type!");
}
template <>
inline bool TryParseType<double>(const std::string& str, double* value) {
  return sscanf(str.c_str(), "%lf", value) == 1;
}
template <>
inline bool TryParseType<int64_t>(const std::string& str, int64_t* value) {
  return sscanf(str.c_str(), "%li", value) == 1;
}
template <>
inline bool TryParseType<bool>(const std::string& str, bool* value) {
  int int_val;
  if (sscanf(str.c_str(), "%i", &int_val) == 1) {
    *value = int_val != 0;
    return true;
  }
  return false;
}
template <>
inline bool TryParseType<std::string>(const std::string& str,
                                      std::string* value) {
  *value = str;
  return true;
}
template <>
inline bool TryParseType<TrialFlag>(const std::string& str, TrialFlag* value) {
  if (str.empty()) {
    *value = TrialFlag::kSet;
    return true;
  }
  return false;
}

int FindOrEnd(std::string str, size_t start, char delimiter) {
  size_t pos = str.find(delimiter, start);
  pos = pos == std::string::npos ? str.length() : pos;
  return static_cast<int>(pos);
}
}  // namespace

TrialFieldInterface::TrialFieldInterface(std::string key) : key_(key) {}
TrialFieldInterface::~TrialFieldInterface() = default;
std::string TrialFieldInterface::Key() const {
  return key_;
}

void ParseFieldTrialFields(std::initializer_list<TrialFieldInterface*> fields,
                           std::string trial_string) {
  std::map<std::string, TrialFieldInterface*> field_map;
  for (TrialFieldInterface* field : fields) {
    field_map[field->Key()] = field;
  }
  size_t i = 0;
  while (i < trial_string.length()) {
    int val_end = FindOrEnd(trial_string, i, ',');
    int colon_pos = FindOrEnd(trial_string, i, ':');
    int key_end = std::min(val_end, colon_pos);
    int val_begin = key_end + 1;
    RTC_LOG(LS_VERBOSE) << trial_string << "   " << val_end << ", " << colon_pos
                        << ", " << key_end << ", " << val_begin;
    std::string key = trial_string.substr(i, key_end - i);
    std::string value = "";
    if (val_end > val_begin)
      value = trial_string.substr(val_begin, val_end - val_begin);
    i = val_end + 1;
    auto field = field_map.find(key);
    if (field != field_map.end()) {
      if (!field->second->TryParse(value)) {
        RTC_LOG(LS_INFO) << "Failed to read field with key: '" << key
                         << "' from \"" << value << "\" in trial: \""
                         << trial_string << "\"";
      }
    } else {
      RTC_LOG(LS_WARNING) << "No field with key: '" << key
                          << "' (found in trial: \"" << trial_string << "\")";
    }
  }
}

template <typename T>
TrialField<T>::TrialField(std::string key, T default_value)
    : TrialFieldInterface(key), value_(default_value) {}

template <typename T>
T TrialField<T>::Get() const {
  return value_;
}

template <typename T>
bool TrialField<T>::TryParse(std::string value_string) {
  return TryParseType<T>(value_string, &value_);
}

template class TrialField<bool>;
template class TrialField<int64_t>;
template class TrialField<double>;
template class TrialField<std::string>;
template class TrialField<TrialFlag>;

TrialFlagField::TrialFlagField(std::string key)
    : TrialField<TrialFlag>(key, TrialFlag::kUnset) {}

bool TrialFlagField::IsSet() const {
  return Get() == TrialFlag::kSet;
}

}  // namespace webrtc
