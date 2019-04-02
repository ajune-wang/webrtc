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
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "rtc_base/string_encode.h"

// Field trial parser functionality. Provides funcitonality to parse field trial
// argument strings in key:value format. Each parameter is described using
// key:value, parameters are separated with a ,. Values can't include the comma
// character, since there's no quote facility. For most types, white space is
// ignored. Parameters are declared with a given type for which an
// implementation of ParseTypedParameter should be provided. The
// ParseTypedParameter implementation is given whatever is between the : and the
// ,. If the key is provided without : a FieldTrialOptional will use nullopt and
// a FieldTrialList will use a empty vector.

// Example string: "my_optional,my_int:3,my_string:hello"

// For further description of usage and behavior, see the examples in the unit
// tests.

namespace webrtc {
class FieldTrialParameterInterface {
 public:
  virtual ~FieldTrialParameterInterface();

 protected:
  // Protected to allow implementations to provide assignment and copy.
  FieldTrialParameterInterface(const FieldTrialParameterInterface&) = default;
  FieldTrialParameterInterface& operator=(const FieldTrialParameterInterface&) =
      default;
  explicit FieldTrialParameterInterface(std::string key);
  friend void ParseFieldTrial(
      std::initializer_list<FieldTrialParameterInterface*> fields,
      std::string raw_string);
  void MarkAsUsed() { used_ = true; }
  virtual bool Parse(absl::optional<std::string> str_value) = 0;
  std::string Key() const;

 private:
  std::string key_;
  bool used_ = false;
};

// ParseFieldTrial function parses the given string and fills the given fields
// with extracted values if available.
void ParseFieldTrial(
    std::initializer_list<FieldTrialParameterInterface*> fields,
    std::string raw_string);

// Specialize this in code file for custom types. Should return absl::nullopt if
// the given string cannot be properly parsed.
template <typename T>
absl::optional<T> ParseTypedParameter(std::string);

// This class uses the ParseTypedParameter function to implement a parameter
// implementation with an enforced default value.
template <typename T>
class FieldTrialParameter : public FieldTrialParameterInterface {
 public:
  FieldTrialParameter(std::string key, T default_value)
      : FieldTrialParameterInterface(key), value_(default_value) {}
  T Get() const { return value_; }
  operator T() const { return Get(); }
  const T* operator->() const { return &value_; }

 protected:
  bool Parse(absl::optional<std::string> str_value) override {
    if (str_value) {
      absl::optional<T> value = ParseTypedParameter<T>(*str_value);
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

// This class uses the ParseTypedParameter function to implement a parameter
// implementation with an enforced default value and a range constraint. Values
// outside the configured range will be ignored.
template <typename T>
class FieldTrialConstrained : public FieldTrialParameterInterface {
 public:
  FieldTrialConstrained(std::string key,
                        T default_value,
                        absl::optional<T> lower_limit,
                        absl::optional<T> upper_limit)
      : FieldTrialParameterInterface(key),
        value_(default_value),
        lower_limit_(lower_limit),
        upper_limit_(upper_limit) {}
  T Get() const { return value_; }
  operator T() const { return Get(); }
  const T* operator->() const { return &value_; }

 protected:
  bool Parse(absl::optional<std::string> str_value) override {
    if (str_value) {
      absl::optional<T> value = ParseTypedParameter<T>(*str_value);
      if (value && (!lower_limit_ || *value >= *lower_limit_) &&
          (!upper_limit_ || *value <= *upper_limit_)) {
        value_ = *value;
        return true;
      }
    }
    return false;
  }

 private:
  T value_;
  absl::optional<T> lower_limit_;
  absl::optional<T> upper_limit_;
};

class AbstractFieldTrialEnum : public FieldTrialParameterInterface {
 public:
  AbstractFieldTrialEnum(std::string key,
                         int default_value,
                         std::map<std::string, int> mapping);
  ~AbstractFieldTrialEnum() override;
  AbstractFieldTrialEnum(const AbstractFieldTrialEnum&);

 protected:
  bool Parse(absl::optional<std::string> str_value) override;

 protected:
  int value_;
  std::map<std::string, int> enum_mapping_;
  std::set<int> valid_values_;
};

// The FieldTrialEnum class can be used to quickly define a parser for a
// specific enum. It handles values provided as integers and as strings if a
// mapping is provided.
template <typename T>
class FieldTrialEnum : public AbstractFieldTrialEnum {
 public:
  FieldTrialEnum(std::string key,
                 T default_value,
                 std::map<std::string, T> mapping)
      : AbstractFieldTrialEnum(key,
                               static_cast<int>(default_value),
                               ToIntMap(mapping)) {}
  T Get() const { return static_cast<T>(value_); }
  operator T() const { return Get(); }

 private:
  static std::map<std::string, int> ToIntMap(std::map<std::string, T> mapping) {
    std::map<std::string, int> res;
    for (const auto& it : mapping)
      res[it.first] = static_cast<int>(it.second);
    return res;
  }
};

// This class uses the ParseTypedParameter function to implement an optional
// parameter implementation that can default to absl::nullopt.
template <typename T>
class FieldTrialOptional : public FieldTrialParameterInterface {
 public:
  explicit FieldTrialOptional(std::string key)
      : FieldTrialParameterInterface(key) {}
  FieldTrialOptional(std::string key, absl::optional<T> default_value)
      : FieldTrialParameterInterface(key), value_(default_value) {}
  absl::optional<T> GetOptional() const { return value_; }
  const T& Value() const { return value_.value(); }
  const T& operator*() const { return value_.value(); }
  const T* operator->() const { return &value_.value(); }
  explicit operator bool() const { return value_.has_value(); }

 protected:
  bool Parse(absl::optional<std::string> str_value) override {
    if (str_value) {
      absl::optional<T> value = ParseTypedParameter<T>(*str_value);
      if (!value.has_value())
        return false;
      value_ = value.value();
    } else {
      value_ = absl::nullopt;
    }
    return true;
  }

 private:
  absl::optional<T> value_;
};

// Equivalent to a FieldTrialParameter<bool> in the case that both key and value
// are present. If key is missing, evaluates to false. If key is present, but no
// explicit value is provided, the flag evaluates to true.
class FieldTrialFlag : public FieldTrialParameterInterface {
 public:
  explicit FieldTrialFlag(std::string key);
  FieldTrialFlag(std::string key, bool default_value);
  bool Get() const;
  operator bool() const;

 protected:
  bool Parse(absl::optional<std::string> str_value) override;

 private:
  bool value_;
};

template <typename T>
struct TypedListWrapper;

// This class represents a vector of type T. The elements are separated by a |
// and parsed using ParseTypedParameter.
template <typename T>
class FieldTrialList : public FieldTrialParameterInterface {
 public:
  FieldTrialList(std::string key) : FieldTrialList(key, {}) {}
  FieldTrialList(std::string key, std::initializer_list<T> default_value)
      : FieldTrialParameterInterface(key),
        failed_(false),
        values_(default_value),
        parse_got_called_(false) {}

  std::vector<T> Get() const { return values_; }
  operator std::vector<T>() const { return Get(); }
  const T& operator[](size_t index) const { return values_[index]; }
  const std::vector<T>* operator->() const { return &values_; }

 protected:
  bool Parse(absl::optional<std::string> str_value) override {
    parse_got_called_ = true;
    if (!str_value) {
      values_.clear();
      return true;
    }

    std::vector<T> new_values;
    std::vector<std::string> tokens;
    rtc::split(str_value.value(), '|', &tokens);

    for (std::string token : tokens) {
      absl::optional<T> value = ParseTypedParameter<T>(token);
      if (!value) {
        failed_ = true;
        return false;
      }
      new_values.push_back(value.value());
    }

    values_ = std::move(new_values);
    return true;
  }
  friend TypedListWrapper<T>;
  bool Failed() const { return failed_; }
  bool Used() const { return parse_got_called_; }

 private:
  bool failed_;
  std::vector<T> values_;
  bool parse_got_called_;
};

class ListWrapper {
 public:
  virtual ~ListWrapper() = default;
  virtual void WriteElement(void* s, int ix) = 0;

  virtual int Length() = 0;
  virtual bool Failed() = 0;
  virtual bool Used() = 0;
};

template <typename T>
struct TypedListWrapper : ListWrapper {
 public:
  TypedListWrapper(FieldTrialList<T>& list, std::function<void(void*, T)> sink)
      : list_(list), sink_(sink) {}

  void WriteElement(void* s, int ix) override { sink_(s, list_[ix]); }

  int Length() override { return list_->size(); }
  bool Failed() override { return list_.Failed(); }
  bool Used() override { return list_.Used(); }

 private:
  const FieldTrialList<T>& list_;
  std::function<void(void*, T)> sink_;
};

// The Traits struct provides type information about lambdas in the template
// expressions below.
template <typename T>
struct Traits : public Traits<decltype(&T::operator())> {};

template <typename ClassType, typename RetType, typename SourceType>
struct Traits<RetType* (ClassType::*)(SourceType*)const> {
  using ret = RetType;
  using src = SourceType;
};

template <typename F,
          typename S = typename Traits<F>::src,
          typename T = typename Traits<F>::ret>
std::unique_ptr<ListWrapper> TLW(FieldTrialList<T>& l, F f) {
  return absl::make_unique<TypedListWrapper<T>>(
      l, [f](void* s, T t) { *f(static_cast<S*>(s)) = t; });
}

// Utility function that combines several FieldTrialLists into a vector of
// structs. Each list needs to be wrapped in a ListWrapper which is created by
// TLW(), see the unit test for examples of use.
template <typename S>
bool combineLists(std::initializer_list<std::unique_ptr<ListWrapper>> l,
                  S defaults,
                  std::vector<S>* out) {
  // Check that all lists that were in the field trial string had the same
  // number of elements. If not we signal an error. We also return an error if
  // any list had parse errors.
  int length = -1;
  for (auto& li : l) {
    if (li->Failed())
      return false;
    else if (!li->Used())
      continue;
    else if (length == -1)
      length = li->Length();
    else if (length != li->Length())
      return false;
  }

  // No values were supplied for any of the lists so we leave the defaults
  // unchanged.
  if (length == -1) {
    return true;
  }

  *out = std::vector<S>(length, defaults);

  for (auto& li : l)
    if (li->Used())
      for (int i = 0; i < length; i++)
        li->WriteElement(&(*out)[i], i);

  return true;
}

// Accepts true, false, else parsed with sscanf %i, true if != 0.
extern template class FieldTrialParameter<bool>;
// Interpreted using sscanf %lf.
extern template class FieldTrialParameter<double>;
// Interpreted using sscanf %i.
extern template class FieldTrialParameter<int>;
// Using the given value as is.
extern template class FieldTrialParameter<std::string>;

extern template class FieldTrialConstrained<double>;
extern template class FieldTrialConstrained<int>;

extern template class FieldTrialOptional<double>;
extern template class FieldTrialOptional<int>;
extern template class FieldTrialOptional<bool>;
extern template class FieldTrialOptional<std::string>;

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARSER_H_
