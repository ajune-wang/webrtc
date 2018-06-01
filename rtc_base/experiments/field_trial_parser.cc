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
  pos = pos == std::string::npos ? str.length() : pos;
  return static_cast<int>(pos);
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
      RTC_LOG(LS_INFO) << "No field with key: '" << key
                       << "' (found in trial: \"" << trial_string << "\")";
    }
  }
}

}  // namespace webrtc
