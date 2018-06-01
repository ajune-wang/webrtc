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

#include <initializer_list>
#include <string>
#include "api/optional.h"

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

void ParseFieldTrial(
    std::initializer_list<FieldTrialParameterInterface*> fields,
    std::string raw_string);

template <typename T>
struct TypedParameterParser {
 public:
  explicit TypedParameterParser(const std::string& str) {
    static_assert(sizeof(T) == 0, "TryParse not implemented for this type.");
  }
  T Get() const;
  bool valid;
};

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

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_
