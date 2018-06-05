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
// argument strings in key:value format. Each parameter is described using
// key:value, parameters are separated with a ,. Whitespace has no special
// treatment and there are no escape characters. Parameters are declared with a
// given type for which an implementation of ParseTypedParameter should be
// provided. The ParseTypedParameter implementation is given whatever is between
// the : and the , or just an empty string if no : is given.

// For further description of usage and behavior, see the examples in the unit
// tests.

namespace webrtc {
class FieldTrialParameterInterface {
 public:
  virtual ~FieldTrialParameterInterface();

 protected:
  explicit FieldTrialParameterInterface(std::string key);
  friend void ParseFieldTrial(
      std::initializer_list<FieldTrialParameterInterface*> fields,
      std::string raw_string);
  virtual bool Parse(const rtc::Optional<std::string>& str_value) = 0;
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
rtc::Optional<T> ParseTypedParameter(const std::string&) {
  static_assert(sizeof(T) == 0, "Parser not implemented for this type.");
}

// This class uses the ParseTypedParameter funciton to implement a parameter
// implementation with an enforced default value.
template <typename T>
class FieldTrialParameter : public FieldTrialParameterInterface {
 public:
  FieldTrialParameter(std::string key, T default_value)
      : FieldTrialParameterInterface(key), value_(default_value) {}
  T Get() const { return value_; }

 protected:
  bool Parse(const rtc::Optional<std::string>& str_value) override {
    if (str_value) {
      rtc::Optional<T> value = ParseTypedParameter<T>(*str_value);
      if (value.has_value()) {
        value_ = value.value();
        return true;
      }
    }
    return false;
  }

 private:
  T value_;
};

// For all ParseTypedParameter specializations, an rtc::Optional version is
// provided by this partial specialization.
template <typename T>
class FieldTrialParameter<rtc::Optional<T>>
    : public FieldTrialParameterInterface {
 public:
  FieldTrialParameter(std::string key, rtc::Optional<T> default_value)
      : FieldTrialParameterInterface(key), value_(default_value) {}
  rtc::Optional<T> Get() const { return value_; }

 protected:
  bool Parse(const rtc::Optional<std::string>& str_value) override {
    if (str_value) {
      rtc::Optional<T> value = ParseTypedParameter<T>(*str_value);
      if (value.has_value()) {
        value_ = value.value();
        return true;
      }
    } else {
      value_ = rtc::nullopt;
      return true;
    }
    return false;
  }

 private:
  rtc::Optional<T> value_;
};

// True if key is in field trial, false otherwise.
class FieldTrialFlag : public FieldTrialParameterInterface {
 public:
  explicit FieldTrialFlag(std::string key);
  bool Get() const;

 protected:
  bool Parse(const rtc::Optional<std::string>& str_value) override;

 private:
  bool value_;
};

// An empty string is considered true. This is useful since the bool can be
// enabled just by including the key example: ",Enabled," -> true.
extern template class FieldTrialParameter<bool>;
extern template class FieldTrialParameter<double>;
extern template class FieldTrialParameter<int>;
extern template class FieldTrialParameter<std::string>;

extern template class FieldTrialParameter<rtc::Optional<double>>;
extern template class FieldTrialParameter<rtc::Optional<int>>;
extern template class FieldTrialParameter<rtc::Optional<bool>>;
extern template class FieldTrialParameter<rtc::Optional<std::string>>;

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_
