/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/stats/rtcstats.h"

#include "rtc_base/stringencode.h"

#include <cstdio>
#include "absl/strings/str_cat.h"

namespace webrtc {

namespace {

// Produces "[a,b,c]". Works for non-vector |RTCStatsMemberInterface::Type|
// types.
template<typename T>
std::string VectorToString(const std::vector<T>& vector) {
  if (vector.empty())
    return "[]";
  std::string s = absl::StrCat("[", rtc::ToString<T>(vector[0]));
  for (size_t i = 1; i < vector.size(); ++i) {
    absl::StrAppend(&s, ",", rtc::ToString<T>(vector[i]));
  }
  absl::StrAppend(&s, "]");
  return s;
}

// Produces "[\"a\",\"b\",\"c\"]". Works for vectors of both const char* and
// std::string element types.
template<typename T>
std::string VectorOfStringsToString(const std::vector<T>& strings) {
  if (strings.empty())
    return "[]";
  std::string s = absl::StrCat("[\"", rtc::ToString<T>(strings[0]), "\"");
  for (size_t i = 1; i < strings.size(); ++i) {
    absl::StrAppend(&s, ",\"", rtc::ToString<T>(strings[i]), "\"");
  }
  absl::StrAppend(&s, "]");
  return s;
}

template <typename T>
std::string ToStringAsDouble(const T value) {
  const size_t buffer_length = 32;
  char buf[buffer_length];
  // JSON represents numbers as floating point numbers with about 15 decimal
  // digits of precision.
  snprintf(buf, buffer_length, "%.16f", static_cast<double>(value));
  return std::string(buf);
}

template <typename T>
std::string VectorToStringAsDouble(const std::vector<T>& vector) {
  if (vector.empty())
    return "[]";
  std::string s = absl::StrCat("[", ToStringAsDouble<T>(vector[0]));
  for (size_t i = 1; i < vector.size(); ++i) {
    absl::StrAppend(&s, ",", ToStringAsDouble<T>(vector[i]));
  }
  absl::StrAppend(&s, "]");
  return s;
}

}  // namespace

bool RTCStats::operator==(const RTCStats& other) const {
  if (type() != other.type() || id() != other.id())
    return false;
  std::vector<const RTCStatsMemberInterface*> members = Members();
  std::vector<const RTCStatsMemberInterface*> other_members = other.Members();
  RTC_DCHECK_EQ(members.size(), other_members.size());
  for (size_t i = 0; i < members.size(); ++i) {
    const RTCStatsMemberInterface* member = members[i];
    const RTCStatsMemberInterface* other_member = other_members[i];
    RTC_DCHECK_EQ(member->type(), other_member->type());
    RTC_DCHECK_EQ(member->name(), other_member->name());
    if (*member != *other_member)
      return false;
  }
  return true;
}

bool RTCStats::operator!=(const RTCStats& other) const {
  return !(*this == other);
}

std::string RTCStats::ToJson() const {
  std::string s = absl::StrCat("{\"type\":\"", type(), "\",", "\"id\":\"", id_,
                               "\",", "\"timestamp\":", timestamp_us_);
  for (const RTCStatsMemberInterface* member : Members()) {
    if (member->is_defined()) {
      absl::StrAppend(&s, ",\"", member->name(), "\":");
      if (member->is_string())
        absl::StrAppend(&s, "\"", member->ValueToJson(), "\"");
      else
        absl::StrAppend(&s, member->ValueToJson());
    }
  }
  absl::StrAppend(&s, "}");
  return s;
}

std::vector<const RTCStatsMemberInterface*> RTCStats::Members() const {
  return MembersOfThisObjectAndAncestors(0);
}

std::vector<const RTCStatsMemberInterface*>
RTCStats::MembersOfThisObjectAndAncestors(
    size_t additional_capacity) const {
  std::vector<const RTCStatsMemberInterface*> members;
  members.reserve(additional_capacity);
  return members;
}

#define WEBRTC_DEFINE_RTCSTATSMEMBER(T, type, is_seq, is_str, to_str, to_json) \
  template <>                                                                  \
  const RTCStatsMemberInterface::Type RTCStatsMember<T>::kType =               \
      RTCStatsMemberInterface::type;                                           \
  template <>                                                                  \
  bool RTCStatsMember<T>::is_sequence() const {                                \
    return is_seq;                                                             \
  }                                                                            \
  template <>                                                                  \
  bool RTCStatsMember<T>::is_string() const {                                  \
    return is_str;                                                             \
  }                                                                            \
  template <>                                                                  \
  std::string RTCStatsMember<T>::ValueToString() const {                       \
    RTC_DCHECK(is_defined_);                                                   \
    return to_str;                                                             \
  }                                                                            \
  template <>                                                                  \
  std::string RTCStatsMember<T>::ValueToJson() const {                         \
    RTC_DCHECK(is_defined_);                                                   \
    return to_json;                                                            \
  }

WEBRTC_DEFINE_RTCSTATSMEMBER(bool,
                             kBool,
                             false,
                             false,
                             rtc::ToString(value_),
                             rtc::ToString(value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(int32_t,
                             kInt32,
                             false,
                             false,
                             rtc::ToString(value_),
                             rtc::ToString(value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(uint32_t,
                             kUint32,
                             false,
                             false,
                             rtc::ToString(value_),
                             rtc::ToString(value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(int64_t,
                             kInt64,
                             false,
                             false,
                             rtc::ToString(value_),
                             ToStringAsDouble(value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(uint64_t,
                             kUint64,
                             false,
                             false,
                             rtc::ToString(value_),
                             ToStringAsDouble(value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(double,
                             kDouble,
                             false,
                             false,
                             rtc::ToString(value_),
                             ToStringAsDouble(value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::string,
                             kString,
                             false,
                             true,
                             value_,
                             value_);
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<bool>,
                             kSequenceBool,
                             true,
                             false,
                             VectorToString(value_),
                             VectorToString(value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<int32_t>,
                             kSequenceInt32,
                             true,
                             false,
                             VectorToString(value_),
                             VectorToString(value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<uint32_t>,
                             kSequenceUint32,
                             true,
                             false,
                             VectorToString(value_),
                             VectorToString(value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<int64_t>,
                             kSequenceInt64,
                             true,
                             false,
                             VectorToString(value_),
                             VectorToStringAsDouble(value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<uint64_t>,
                             kSequenceUint64,
                             true,
                             false,
                             VectorToString(value_),
                             VectorToStringAsDouble(value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<double>,
                             kSequenceDouble,
                             true,
                             false,
                             VectorToString(value_),
                             VectorToStringAsDouble(value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<std::string>,
                             kSequenceString,
                             true,
                             false,
                             VectorOfStringsToString(value_),
                             VectorOfStringsToString(value_));

}  // namespace webrtc
