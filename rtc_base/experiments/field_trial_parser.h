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

namespace webrtc {

// Special enum used for indicating presence of a key in the field trial string.
// For example: "Enabled".
enum class TrialFlag { kUnset, kSet };

class TrialFieldInterface {
 public:
  virtual ~TrialFieldInterface();

 protected:
  explicit TrialFieldInterface(std::string key);
  friend void ParseFieldTrialFields(
      std::initializer_list<TrialFieldInterface*> fields,
      std::string raw_string);
  virtual bool TryParse(std::string value) = 0;
  std::string Key() const;

 private:
  const std::string key_;
};

void ParseFieldTrialFields(std::initializer_list<TrialFieldInterface*> fields,
                           std::string raw_string);

template <typename T>
class TrialField : public TrialFieldInterface {
 public:
  TrialField(std::string key, T default_value);
  T Get() const;

 protected:
  bool TryParse(std::string value_string) override;

 private:
  T value_;
};

extern template class TrialField<double>;
extern template class TrialField<int64_t>;
extern template class TrialField<bool>;
extern template class TrialField<std::string>;
extern template class TrialField<TrialFlag>;

class TrialFlagField : public TrialField<TrialFlag> {
 public:
  explicit TrialFlagField(std::string key);
  bool IsSet() const;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_
