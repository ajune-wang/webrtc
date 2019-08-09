/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_STRUCT_PARAMETERS_PARSER_H_
#define RTC_BASE_EXPERIMENTS_STRUCT_PARAMETERS_PARSER_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace struct_parser_impl {
inline void StringEncode(rtc::StringBuilder* sb, bool val) {
  *sb << rtc::ToString(val);
}
inline void StringEncode(rtc::StringBuilder* sb, double val) {
  *sb << val;
}
inline void StringEncode(rtc::StringBuilder* sb, int val) {
  *sb << val;
}
inline void StringEncode(rtc::StringBuilder* sb, const std::string& val) {
  *sb << val;
}
inline void StringEncode(rtc::StringBuilder* sb, DataRate val) {
  *sb << ToString(val);
}
inline void StringEncode(rtc::StringBuilder* sb, DataSize val) {
  *sb << ToString(val);
}
inline void StringEncode(rtc::StringBuilder* sb, TimeDelta val) {
  *sb << ToString(val);
}

template <typename T>
inline void StringEncode(rtc::StringBuilder* sb, absl::optional<T> val) {
  if (val)
    StringEncode(sb, *val);
}

template <typename T>
struct LambdaTraits : public LambdaTraits<decltype(&T::operator())> {};

template <typename ClassType, typename RetType, typename SourceType>
struct LambdaTraits<RetType* (ClassType::*)(SourceType*)const> {
  using ret = RetType;
  using src = SourceType;
};

struct ParameterParser {
 public:
  absl::string_view key;
  size_t member_offset;
  bool (*parse)(const absl::string_view src, void* target);
  bool (*changed)(const void* src, const void* base);
  void (*encode)(const void* src, rtc::StringBuilder* target);

  bool Parse(const absl::string_view src, void* target_struct) const {
    return parse(src, MemberPtr(target_struct));
  }
  bool Changed(const void* src_struct, const void* base_struct) const {
    return changed(MemberPtr(src_struct), MemberPtr(base_struct));
  }
  void Encode(const void* src_struct, rtc::StringBuilder* target) const {
    encode(MemberPtr(src_struct), target);
  }

 private:
  void* MemberPtr(void* struct_ptr) const {
    return reinterpret_cast<char*>(struct_ptr) + member_offset;
  }
  const void* MemberPtr(const void* struct_ptr) const {
    return reinterpret_cast<const char*>(struct_ptr) + member_offset;
  }
};

class ParserBase {
 public:
  explicit ParserBase(std::vector<ParameterParser> fields);
  void Parse(void* target, absl::string_view src) const;
  std::string EncodeChanged(const void* src, const void* base) const;
  std::string EncodeAll(const void* src) const;

 private:
  std::vector<ParameterParser> fields_;
};

template <typename T>
bool TypedParserParse(absl::string_view src, void* target) {
  auto parsed = ParseTypedParameter<T>(std::string(src));
  if (parsed.has_value())
    *reinterpret_cast<T*>(target) = *parsed;
  return parsed.has_value();
}
template <typename T>
bool TypedParserChanged(const void* src, const void* base) {
  return *reinterpret_cast<const T*>(src) != *reinterpret_cast<const T*>(base);
}
template <typename T>
void TypedParserEncode(const void* src, rtc::StringBuilder* target) {
  StringEncode(target, *reinterpret_cast<const T*>(src));
}

template <typename T>
ParameterParser TypedParser(const char* key, void* base, T* member) {
  return ParameterParser{
      key,
      static_cast<size_t>(reinterpret_cast<const char*>(member) -
                          reinterpret_cast<const char*>(base)),
      &TypedParserParse<T>, &TypedParserChanged<T>, &TypedParserEncode<T>};
}

template <typename S,
          typename Closure,
          typename T = typename LambdaTraits<Closure>::ret>
void AddParameters(ParameterParser* out,
                   S* base,
                   const char* key,
                   Closure getter) {
  *out = TypedParser<T>(key, base, getter(base));
}

template <typename S,
          typename Closure,
          typename T = typename LambdaTraits<Closure>::ret,
          typename... Args>
void AddParameters(ParameterParser* out,
                   S* base,
                   const char* key,
                   Closure getter,
                   Args... args) {
  AddParameters<S>(out, base, key, getter);
  AddParameters<S>(++out, base, args...);
}

}  // namespace struct_parser_impl

template <typename StructType>
class StructParametersParser {
 public:
  void Parse(StructType* target, absl::string_view src) {
    base_parser_.Parse(target, src);
  }
  StructType Parse(absl::string_view src) {
    StructType res;
    Parse(&res, src);
    return res;
  }
  std::string EncodeChanged(const StructType& src) {
    static StructType base;
    return base_parser_.EncodeChanged(&src, &base);
  }
  std::string EncodeAll(const StructType& src) {
    return base_parser_.EncodeAll(&src);
  }

 private:
  template <typename C, typename S, typename... Args>
  friend StructParametersParser<S>* CreateStructParametersParser(const char*,
                                                                 C,
                                                                 Args...);

  explicit StructParametersParser(
      std::vector<struct_parser_impl::ParameterParser> parameters)
      : base_parser_(std::move(parameters)) {}
  const struct_parser_impl::ParserBase base_parser_;
};

// Creates a struct parameters parser based on interleaved key and field
// accessor arguments, where the field accessor converts a struct pointer to a
// member pointer: FieldType*(StructType*). See the unit tests for example
// usage. Note that the struct type is inferred from the field getters. Beware
// of providing incorrect arguments to this, such as mixing the struct type or
// incorrect return values, as this will cause very confusing compile errors.
// It returns a raw pointer to allow it to be assigned as a static member to
// avoid repeated construction cost.
template <typename Closure,
          typename S = typename struct_parser_impl::LambdaTraits<Closure>::src,
          typename... Args>
StructParametersParser<S>* CreateStructParametersParser(const char* first_key,
                                                        Closure first_getter,
                                                        Args... args) {
  std::vector<struct_parser_impl::ParameterParser> parameters(
      sizeof...(args) / 2 + 1);
  S base;
  struct_parser_impl::AddParameters<S>(
      &parameters.front(), &base, std::move(first_key), first_getter, args...);
  return new StructParametersParser<S>(std::move(parameters));
}
}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_STRUCT_PARAMETERS_PARSER_H_
