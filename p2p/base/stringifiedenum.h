/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_STRINGIFIEDENUM_H_
#define P2P_BASE_STRINGIFIEDENUM_H_

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

// This file defines the macros to define scoped enumerated type, of which the
// enumerated values can be stringified using a user-defined formatter.
//
// The defined enum type has two helper methods, EnumToStr and StrToEnum,
// which can
//  1) stringify the enumerated value to a corresponding string
//  representation and also
//  2) translate a string representation to an enumerated value if such a
//  mapping exists; otherwise this string is recorded for reference in case
//  any ad-hoc value can appear in tests and applications.
//
// The stringifying rule from an enumerated value to a string is given by
// the user and the string-to-enum inverse mapping is automatically generated.
//
// Usage:
// 1. Declare an scoped enum using the statement
// DECLARE_STRINGIFIED_ENUM(name-of-the-enum, enum-val-1, enum-val-2);
// e.g.,
// DECLARE_STRINGIFIED_ENUM(Fruit, kApple, kBanana, kCranberry);
//
// 2. The above declaration exports enumerated values that can be used as scoped
// enum as enum class in C+11 e.g., Fruit::kApple
//
// 3. Define the above scoped enum using the statement
// DEFINE_STRINGIFIED_ENUM(name-of-the-enum, enum-val-1, enum-val-2);
// where the argument list should be identical to the declaration.
//
// 4. After the definition of the enum, the stringified enum name can be
// obtained using the EnumToStr method, e.g.,
// Fruit::EnumToStr(Fruit::kApple), which returns "apple"

namespace webrtc {

namespace icelog {

// tokenize an arguments string "arg1, arg2, ..., argN" to
// {"arg1", "arg2", ... "argN"}
std::vector<std::string> TokenizeArgString(const std::string& args_str);

// the default formatter that reformats the string "kNameInCamelCase" generated
// from naming convention of enum values to an "nameInCamelCase" string
auto const defaultFormatter = [](std::string s) {
  if (!s.empty() && s[0] == 'k') {
    s = s.substr(1);
  }
  s[0] = ::tolower(s[0]);
  return s;
};

}  // namespace icelog

}  // namespace webrtc

#define DEFINE_VARIADIC_ENUM(enum_name, ...) \
  enum enum_name { kUndefined = 0, __VA_ARGS__, kNumElementsPlusOne }

// Implementation detail via the example
// DEFINE_STRINGIFIED_ENUM(Fruit, kApple, kBanana, kCranberry)
//
// The string "kApple, kBanana, kCranberry" is stored as the basis for
// reflection, which converts it to an string array
// {"kApple", "kBanana", "kCranberry"}, which is further reformatted by the
// formatter to, e.g. by default {"apple", "banana", "cranberry"} and
// the mapping between enum and string is populated afterwards
#define DECLARE_STRINGIFIED_ENUM(enum_name, ...)                        \
  class enum_name {                                                     \
   public:                                                              \
    DEFINE_VARIADIC_ENUM(Value, __VA_ARGS__);                           \
    static std::map<Value, std::string> etos;                           \
    static std::map<std::string, Value> stoe;                           \
    static std::string EnumToStr(Value enum_val);                       \
    static Value StrToEnum(const std::string& str);                     \
    static std::string undefined_encountered();                         \
    virtual enum_name& operator=(const enum_name& other) {              \
      value_ = other.value();                                           \
      return *this;                                                     \
    }                                                                   \
    virtual enum_name& operator=(Value other_value) {                   \
      value_ = other_value;                                             \
      return *this;                                                     \
    }                                                                   \
    virtual bool operator==(const enum_name& other) const {             \
      return value_ == other.value();                                   \
    }                                                                   \
    explicit enum_name(const enum_name& other) { operator=(other); }    \
    explicit enum_name(Value other_value) { operator=(other_value); }   \
    Value value() const { return value_; }                              \
    using Formatter = std::function<std::string(std::string&)>;         \
    void set_formatter(Formatter formatter) { formatter_ = formatter; } \
    virtual ~enum_name() {}                                             \
                                                                        \
   protected:                                                           \
    static bool reflected_;                                             \
    static std::set<std::string> undefined_set_str_;                    \
    static Formatter formatter_;                                        \
    Value value_;                                                       \
    static void Reflect();                                              \
  };

#define DEFINE_STRINGIFIED_ENUM(enum_name, ...)                   \
  bool enum_name::reflected_ = false;                             \
  enum_name::Formatter enum_name::formatter_ = defaultFormatter;  \
  std::map<enum_name::Value, std::string> enum_name::etos = {};   \
  std::map<std::string, enum_name::Value> enum_name::stoe = {};   \
  std::set<std::string> enum_name::undefined_set_str_;            \
  void enum_name::Reflect() {                                     \
    reflected_ = true;                                            \
    std::string cs(#__VA_ARGS__);                                 \
    std::vector<std::string> enum_val_tokens =                    \
        webrtc::icelog::TokenizeArgString(cs);                    \
    etos[enum_name::kUndefined] = "undefined";                    \
    for (int i = 1; i < kNumElementsPlusOne; i++) {               \
      enum_name::Value e = (enum_name::Value)i;                   \
      std::string s = formatter_(enum_val_tokens[i - 1]);         \
      etos[e] = s;                                                \
      stoe[s] = e;                                                \
    }                                                             \
  }                                                               \
  std::string enum_name::EnumToStr(enum_name::Value enum_val) {   \
    if (!reflected_) {                                            \
      enum_name::Reflect();                                       \
    }                                                             \
    if (enum_val == enum_name::kUndefined) {                      \
      return undefined_encountered();                             \
    }                                                             \
    return etos[enum_val];                                        \
  }                                                               \
  enum_name::Value enum_name::StrToEnum(const std::string& str) { \
    if (!reflected_) {                                            \
      enum_name::Reflect();                                       \
    }                                                             \
    if (stoe.find(str) != stoe.end()) {                           \
      return stoe[str];                                           \
    }                                                             \
    undefined_set_str_.insert(str.empty() ? "null" : str);        \
    return enum_name::kUndefined;                                 \
  }                                                               \
  std::string enum_name::undefined_encountered() {                \
    std::string ret;                                              \
    for (std::string s : undefined_set_str_) {                    \
      ret += s + ", ";                                            \
    }                                                             \
    ret = ret.substr(0, ret.size() - 2);                          \
    return ret;                                                   \
  }

#endif  // P2P_BASE_STRINGIFIEDENUM_H_
