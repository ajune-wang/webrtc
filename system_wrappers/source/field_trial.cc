// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//

#include "system_wrappers/include/field_trial.h"

#include <stddef.h>

#include <map>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

// Simple field trial implementation, which allows client to
// specify desired flags in InitFieldTrialsFromString.
namespace webrtc {
namespace field_trial {

static const char* trials_init_string = NULL;
constexpr char kPersistentStringSeparator = '/';

#ifndef WEBRTC_EXCLUDE_FIELD_TRIAL_DEFAULT
// Validates the given field trial string.
//  E.g.:
//    "WebRTC-experimentFoo/Enabled/WebRTC-experimentBar/Enabled100kbps/"
//    Assigns the process to group "Enabled" on WebRTCExperimentFoo trial
//    and to group "Enabled100kbps" on WebRTCExperimentBar.
//
//  E.g. invalid config:
//    "WebRTC-experiment1/Enabled"  (note missing / separator at the end).
//
// Note: This method crashes with an error message if an invalid config is
// passed to it. That can be used to find out if a binary is parsing the flags.
void ValidateFieldTrialsStringOrDie(const std::string& trials_string) {
  if (trials_string.empty())
    return;

  RTC_CHECK(trials_init_string == NULL)
      << "Field trials string set more than once";

  size_t next_item = 0;
  std::map<std::string, std::string> field_trials;
  while (next_item < trials_string.length()) {
    size_t name_end = trials_string.find(kPersistentStringSeparator, next_item);
    if (name_end == trials_string.npos || next_item == name_end)
      break;
    size_t group_name_end =
        trials_string.find(kPersistentStringSeparator, name_end + 1);
    if (group_name_end == trials_string.npos || name_end + 1 == group_name_end)
      break;
    std::string name(trials_string, next_item, name_end - next_item);
    std::string group_name(trials_string, name_end + 1,
                           group_name_end - name_end - 1);
    next_item = group_name_end + 1;

    // Fail if duplicate with different group name.
    if (field_trials.find(name) != field_trials.end() &&
        field_trials.find(name)->second != group_name) {
      break;
    }

    field_trials[name] = group_name;

    // Successfully parsed all field trials from the string.
    if (next_item == trials_string.length()) {
      return;
    }
  }

  FATAL() << "Invalid field trials string:" << trials_string;
}

std::string FindFullName(const std::string& name) {
  if (trials_init_string == NULL)
    return std::string();

  std::string trials_string(trials_init_string);
  if (trials_string.empty())
    return std::string();

  size_t next_item = 0;
  while (next_item < trials_string.length()) {
    // Find next name/value pair in field trial configuration string.
    size_t field_name_end =
        trials_string.find(kPersistentStringSeparator, next_item);
    if (field_name_end == trials_string.npos || field_name_end == next_item)
      break;
    size_t field_value_end =
        trials_string.find(kPersistentStringSeparator, field_name_end + 1);
    if (field_value_end == trials_string.npos ||
        field_value_end == field_name_end + 1)
      break;
    std::string field_name(trials_string, next_item,
                           field_name_end - next_item);
    std::string field_value(trials_string, field_name_end + 1,
                            field_value_end - field_name_end - 1);
    next_item = field_value_end + 1;

    if (name == field_name)
      return field_value;
  }
  return std::string();
}
#endif  // WEBRTC_EXCLUDE_FIELD_TRIAL_DEFAULT

// Optionally initialize field trial from a string.
void InitFieldTrialsFromString(const char* trials_string) {
  RTC_LOG(LS_INFO) << "Setting field trial string:" << trials_init_string;
#ifndef WEBRTC_EXCLUDE_FIELD_TRIAL_DEFAULT
  ValidateFieldTrialsStringOrDie(trials_string);
#endif  // WEBRTC_EXCLUDE_FIELD_TRIAL_DEFAULT
  trials_init_string = trials_string;
}

const char* GetFieldTrialString() {
  return trials_init_string;
}

}  // namespace field_trial
}  // namespace webrtc
