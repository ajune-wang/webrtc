/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/stats/rtc_stats.h"

#include <cstdio>

#include "rtc_base/arraysize.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

namespace {

// Produces "[a,b,c]". Works for non-vector |RTCStatsMemberInterface::Type|
// types.
template <typename T>
std::string VectorToString(const std::vector<T>& vector) {
  rtc::StringBuilder sb;
  sb << "[";
  const char* separator = "";
  for (const T& element : vector) {
    sb << separator << rtc::ToString(element);
    separator = ",";
  }
  sb << "]";
  return sb.Release();
}

// Produces "[\"a\",\"b\",\"c\"]". Works for vectors of both const char* and
// std::string element types.
template <typename T>
std::string VectorOfStringsToString(const std::vector<T>& strings) {
  rtc::StringBuilder sb;
  sb << "[";
  const char* separator = "";
  for (const T& element : strings) {
    sb << separator << "\"" << rtc::ToString(element) << "\"";
    separator = ",";
  }
  sb << "]";
  return sb.Release();
}

template <typename T>
std::string ToStringAsDouble(const T value) {
  // JSON represents numbers as floating point numbers with about 15 decimal
  // digits of precision.
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%.16g",
                                static_cast<double>(value));
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}

template <typename T>
std::string VectorToStringAsDouble(const std::vector<T>& vector) {
  rtc::StringBuilder sb;
  sb << "[";
  const char* separator = "";
  for (const T& element : vector) {
    sb << separator << ToStringAsDouble<T>(element);
    separator = ",";
  }
  sb << "]";
  return sb.Release();
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
  rtc::StringBuilder sb;
  sb << "{\"type\":\"" << type() << "\","
     << "\"id\":\"" << id_ << "\","
     << "\"timestamp\":" << timestamp_us_;
  for (const RTCStatsMemberInterface* member : Members()) {
    if (member->is_defined()) {
      sb << ",\"" << member->name() << "\":";
      if (member->is_string())
        sb << "\"" << member->ValueToJson() << "\"";
      else
        sb << member->ValueToJson();
    }
  }
  sb << "}";
  return sb.Release();
}

std::vector<const RTCStatsMemberInterface*> RTCStats::Members() const {
  return MembersOfThisObjectAndAncestors(0);
}

std::vector<const RTCStatsMemberInterface*>
RTCStats::MembersOfThisObjectAndAncestors(size_t additional_capacity) const {
  std::vector<const RTCStatsMemberInterface*> members;
  members.reserve(additional_capacity);
  return members;
}

bool IsRTCStatsMemberSequence(bool v) {
  return false;
}
bool IsRTCStatsMemberSequence(int32_t v) {
  return false;
}
bool IsRTCStatsMemberSequence(uint32_t v) {
  return false;
}
bool IsRTCStatsMemberSequence(int64_t v) {
  return false;
}
bool IsRTCStatsMemberSequence(uint64_t v) {
  return false;
}
bool IsRTCStatsMemberSequence(double v) {
  return false;
}
bool IsRTCStatsMemberSequence(std::string v) {
  return false;
}
bool IsRTCStatsMemberSequence(std::vector<bool> v) {
  return true;
}
bool IsRTCStatsMemberSequence(std::vector<int32_t> v) {
  return true;
}
bool IsRTCStatsMemberSequence(std::vector<uint32_t> v) {
  return true;
}
bool IsRTCStatsMemberSequence(std::vector<int64_t> v) {
  return true;
}
bool IsRTCStatsMemberSequence(std::vector<uint64_t> v) {
  return true;
}
bool IsRTCStatsMemberSequence(std::vector<double> v) {
  return true;
}
bool IsRTCStatsMemberSequence(std::vector<std::string> v) {
  return true;
}

bool IsRTCStatsMemberString(bool v) {
  return false;
}
bool IsRTCStatsMemberString(int32_t v) {
  return false;
}
bool IsRTCStatsMemberString(uint32_t v) {
  return false;
}
bool IsRTCStatsMemberString(int64_t v) {
  return false;
}
bool IsRTCStatsMemberString(uint64_t v) {
  return false;
}
bool IsRTCStatsMemberString(double v) {
  return false;
}
bool IsRTCStatsMemberString(std::string v) {
  return true;
}
bool IsRTCStatsMemberString(std::vector<bool> v) {
  return false;
}
bool IsRTCStatsMemberString(std::vector<int32_t> v) {
  return false;
}
bool IsRTCStatsMemberString(std::vector<uint32_t> v) {
  return false;
}
bool IsRTCStatsMemberString(std::vector<int64_t> v) {
  return false;
}
bool IsRTCStatsMemberString(std::vector<uint64_t> v) {
  return false;
}
bool IsRTCStatsMemberString(std::vector<double> v) {
  return false;
}
bool IsRTCStatsMemberString(std::vector<std::string> v) {
  return false;
}

std::string RTCStatsMemberToString(bool v) {
  return rtc::ToString(v);
}
std::string RTCStatsMemberToString(int32_t v) {
  return rtc::ToString(v);
}
std::string RTCStatsMemberToString(uint32_t v) {
  return rtc::ToString(v);
}
std::string RTCStatsMemberToString(int64_t v) {
  return rtc::ToString(v);
}
std::string RTCStatsMemberToString(uint64_t v) {
  return rtc::ToString(v);
}
std::string RTCStatsMemberToString(double v) {
  return rtc::ToString(v);
}
std::string RTCStatsMemberToString(std::string v) {
  return v;
}
std::string RTCStatsMemberToString(std::vector<bool> v) {
  return VectorToString(v);
}
std::string RTCStatsMemberToString(std::vector<int32_t> v) {
  return VectorToString(v);
}
std::string RTCStatsMemberToString(std::vector<uint32_t> v) {
  return VectorToString(v);
}
std::string RTCStatsMemberToString(std::vector<int64_t> v) {
  return VectorToString(v);
}
std::string RTCStatsMemberToString(std::vector<uint64_t> v) {
  return VectorToString(v);
}
std::string RTCStatsMemberToString(std::vector<double> v) {
  return VectorToString(v);
}
std::string RTCStatsMemberToString(std::vector<std::string> v) {
  return VectorOfStringsToString(v);
}

std::string RTCStatsMemberToJson(bool v) {
  return rtc::ToString(v);
}
std::string RTCStatsMemberToJson(int32_t v) {
  return rtc::ToString(v);
}
std::string RTCStatsMemberToJson(uint32_t v) {
  return rtc::ToString(v);
}
std::string RTCStatsMemberToJson(int64_t v) {
  return ToStringAsDouble(v);
}
std::string RTCStatsMemberToJson(uint64_t v) {
  return ToStringAsDouble(v);
}
std::string RTCStatsMemberToJson(double v) {
  return ToStringAsDouble(v);
}
std::string RTCStatsMemberToJson(std::string v) {
  return v;
}
std::string RTCStatsMemberToJson(std::vector<bool> v) {
  return VectorToString(v);
}
std::string RTCStatsMemberToJson(std::vector<int32_t> v) {
  return VectorToString(v);
}
std::string RTCStatsMemberToJson(std::vector<uint32_t> v) {
  return VectorToString(v);
}
std::string RTCStatsMemberToJson(std::vector<int64_t> v) {
  return VectorToStringAsDouble(v);
}
std::string RTCStatsMemberToJson(std::vector<uint64_t> v) {
  return VectorToStringAsDouble(v);
}
std::string RTCStatsMemberToJson(std::vector<double> v) {
  return VectorToStringAsDouble(v);
}
std::string RTCStatsMemberToJson(std::vector<std::string> v) {
  return VectorOfStringsToString(v);
}

#define WEBRTC_DEFINE_RTCSTATSMEMBER(T, type)                  \
  template <>                                                  \
  RTCStatsMemberInterface::Type RTCStatsMember<T>::GetType() { \
    return type;                                               \
  }

WEBRTC_DEFINE_RTCSTATSMEMBER(bool, kBool)
WEBRTC_DEFINE_RTCSTATSMEMBER(int32_t, kInt32)
WEBRTC_DEFINE_RTCSTATSMEMBER(uint32_t, kUint32)
WEBRTC_DEFINE_RTCSTATSMEMBER(int64_t, kInt64)
WEBRTC_DEFINE_RTCSTATSMEMBER(uint64_t, kUint64)
WEBRTC_DEFINE_RTCSTATSMEMBER(double, kDouble)
WEBRTC_DEFINE_RTCSTATSMEMBER(std::string, kString)
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<bool>, kSequenceBool)
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<int32_t>, kSequenceInt32)
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<uint32_t>, kSequenceUint32)
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<int64_t>, kSequenceInt64)
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<uint64_t>, kSequenceUint64)
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<double>, kSequenceDouble)
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<std::string>, kSequenceString)

}  // namespace webrtc
