/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_
#define RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_

#include <stdint.h>
#include <initializer_list>
#include <string>
#include "api/optional.h"

// Field trial parser functionality. Provides funcitonality to parse field trial
// argument strings in key:value format.
//
// For  recommended usage, look at the unit tests.

namespace webrtc {
class FieldTrialParameterInterface {
 public:
  virtual ~FieldTrialParameterInterface();

 protected:
  explicit FieldTrialParameterInterface(std::string key);
  friend void ParseFieldTrial(
      std::initializer_list<FieldTrialParameterInterface*> fields,
      std::string raw_string);
  virtual bool TryParse(std::string value) = 0;
  std::string Key() const;

 private:
  const std::string key_;
};

// ParseFieldTrial function parses the given string and fills the given fields
// with extrated values if available.
void ParseFieldTrial(
    std::initializer_list<FieldTrialParameterInterface*> fields,
    std::string raw_string);

// Specialize this in code file for custom types. The Get function must return a
// proper value if public |valid| field is true after initialization.
template <typename T>
struct TypedParameterParser {
 public:
  explicit TypedParameterParser(const std::string& str) {
    static_assert(sizeof(T) == 0, "TryParse not implemented for this type.");
  }
  T Get() const;
  bool valid;
};

// For all TypedParameterParser sopecializations, an rtc::Optional version is
// provided by this partial specialization.
template <typename T>
struct TypedParameterParser<rtc::Optional<T>> {
  explicit TypedParameterParser(const std::string& str) {
    if (str.empty() || str == "nan") {
      value = rtc::nullopt;
      valid = true;
    } else {
      TypedParameterParser<T> parser(str);
      if (parser.valid) {
        value = parser.Get();
        valid = true;
      }
    }
  }
  rtc::Optional<T> Get() const { return value; }
  bool valid = false;
  rtc::Optional<T> value;
};

// This class uses the TypedParameterParser to implement a parameter
// implementation with an enforced default value.
template <typename T>
class FieldTrialParameter : public FieldTrialParameterInterface {
 public:
  FieldTrialParameter(std::string key, T default_value)
      : FieldTrialParameterInterface(key), value_(default_value) {}
  T Get() const { return value_; }

 protected:
  bool TryParse(std::string value_string) override {
    TypedParameterParser<T> parser(value_string);
    if (parser.valid)
      value_ = parser.Get();
    return parser.valid;
  }

 private:
  T value_;
};

// An empty string is considered true. This is useful since the bool can be
// enabled just by including the key example: ",Enabled," -> true.
extern template class FieldTrialParameter<bool>;
extern template class FieldTrialParameter<double>;
extern template class FieldTrialParameter<int64_t>;
extern template class FieldTrialParameter<std::string>;

extern template class FieldTrialParameter<rtc::Optional<double>>;
extern template class FieldTrialParameter<rtc::Optional<int64_t>>;
extern template class FieldTrialParameter<rtc::Optional<bool>>;

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_
