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
struct LambdaTraits<RetType (ClassType::*)(SourceType*) const> {
  using src = SourceType;
};

struct TypedMemberParser {
 public:
  bool (*parse)(const absl::string_view src, void* target);
  bool (*changed)(const void* src, const void* base);
  void (*encode)(const void* src, rtc::StringBuilder* target);
};

struct StructFieldEntry {
  absl::string_view key;
  size_t member_offset;
  TypedMemberParser parser;
};

struct MemberParameter {
  absl::string_view key;
  void* member_ptr;
  TypedMemberParser parser;

  bool Parse(const absl::string_view src) {
    return parser.parse(src, member_ptr);
  }
  bool Changed(const void* src_struct, const void* base_struct) const {
    return parser.changed(member_ptr, BasePtr(src_struct, base_struct));
  }
  void Encode(rtc::StringBuilder* target) const {
    parser.encode(member_ptr, target);
  }

 private:
  const void* BasePtr(const void* src_struct, const void* base_struct) const {
    size_t member_offset =
        static_cast<size_t>(reinterpret_cast<const char*>(member_ptr) -
                            reinterpret_cast<const char*>(src_struct));
    return reinterpret_cast<const char*>(base_struct) + member_offset;
  }
};

class ParserBase {
 public:
  ParserBase(const void* const base,
             void* const target,
             std::vector<MemberParameter> members);
  void Parse(absl::string_view src);
  std::string EncodeChanged() const;
  std::string EncodeAll() const;

 private:
  const void* const base_;
  void* const target_;
  std::vector<MemberParameter> fields_;
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
void AddMembers(MemberParameter* out, const char* key, T* member) {
  *out = MemberParameter{
      key, member,
      TypedMemberParser{&TypedParserParse<T>, &TypedParserChanged<T>,
                        &TypedParserEncode<T>}};
}

template <typename T, typename... Args>
void AddMembers(MemberParameter* out,
                const char* key,
                T* member,
                Args... args) {
  AddMembers(out, key, member);
  AddMembers(++out, args...);
}
}  // namespace struct_parser_impl

class StructParametersParser : public struct_parser_impl::ParserBase {
 private:
  template <typename S, typename T, typename... Args>
  StructParametersParser* CreateStructParametersParser(S*,
                                                       const char*,
                                                       T*,
                                                       Args...);
  using struct_parser_impl::ParserBase::ParserBase;
};

// Creates a struct parameters parser based on interleaved key and field
// accessor arguments, where the field accessor converts a struct pointer to a
// member pointer: FieldType*(StructType*). See the unit tests for example
// usage. Note that the struct type is inferred from the field getters. Beware
// of providing incorrect arguments to this, such as mixing the struct type or
// incorrect return values, as this will cause very confusing compile errors.
// It returns a raw pointer to allow it to be assigned as a static member to
// avoid repeated construction cost.

template <typename S, typename T, typename... Args>
std::unique_ptr<StructParametersParser> CreateStructParametersParser(
    S* target,
    const char* first_key,
    T* first_member,
    Args... args) {
  static S* defauts = new S();
  std::vector<struct_parser_impl::MemberParameter> members(sizeof...(args) / 2 +
                                                           1);
  struct_parser_impl::AddMembers(&members.front(), std::move(first_key),
                                 first_member, args...);
  return absl::make_unique<StructParametersParser>(defauts, target,
                                                   std::move(members));
}

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_STRUCT_PARAMETERS_PARSER_H_
