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

namespace webrtc {

namespace {}  // namespace

namespace rtc_stats_internal {
std::string VectorToString(const std::vector<bool>& vector) {
  rtc::StringBuilder sb;
  sb << "[";
  const char* separator = "";
  for (bool element : vector) {
    sb << separator << rtc::ToString(element);
    separator = ",";
  }
  sb << "]";
  return sb.Release();
}
}  // namespace rtc_stats_internal

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
  sb << "{\"type\":\"" << type()
     << "\","
        "\"id\":\""
     << id_
     << "\","
        "\"timestamp\":"
     << timestamp_us_;
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

// WEBRTC_DEFINE_RTCSTATSMEMBER(bool,
//                              kBool,
//                              false,
//                              false,
//                              rtc::ToString(value_),
//                              rtc::ToString(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(int32_t,
//                              kInt32,
//                              false,
//                              false,
//                              rtc::ToString(value_),
//                              rtc::ToString(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(uint32_t,
//                              kUint32,
//                              false,
//                              false,
//                              rtc::ToString(value_),
//                              rtc::ToString(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(int64_t,
//                              kInt64,
//                              false,
//                              false,
//                              rtc::ToString(value_),
//                              ToStringAsDouble(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(uint64_t,
//                              kUint64,
//                              false,
//                              false,
//                              rtc::ToString(value_),
//                              ToStringAsDouble(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(double,
//                              kDouble,
//                              false,
//                              false,
//                              rtc::ToString(value_),
//                              ToStringAsDouble(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(std::string, kString, false, true, value_,
// value_); WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<bool>,
//                              kSequenceBool,
//                              true,
//                              false,
//                              VectorToString(value_),
//                              VectorToString(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<int32_t>,
//                              kSequenceInt32,
//                              true,
//                              false,
//                              VectorToString(value_),
//                              VectorToString(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<uint32_t>,
//                              kSequenceUint32,
//                              true,
//                              false,
//                              VectorToString(value_),
//                              VectorToString(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<int64_t>,
//                              kSequenceInt64,
//                              true,
//                              false,
//                              VectorToString(value_),
//                              VectorToStringAsDouble(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<uint64_t>,
//                              kSequenceUint64,
//                              true,
//                              false,
//                              VectorToString(value_),
//                              VectorToStringAsDouble(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<double>,
//                              kSequenceDouble,
//                              true,
//                              false,
//                              VectorToString(value_),
//                              VectorToStringAsDouble(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<std::string>,
//                              kSequenceString,
//                              true,
//                              false,
//                              VectorOfStringsToString(value_),
//                              VectorOfStringsToString(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(rtc_stats_internal::MapStringUint64,
//                              kMapStringUint64,
//                              false,
//                              false,
//                              MapToString(value_),
//                              MapToStringAsDouble(value_));
// WEBRTC_DEFINE_RTCSTATSMEMBER(rtc_stats_internal::MapStringDouble,
//                              kMapStringDouble,
//                              false,
//                              false,
//                              MapToString(value_),
//                              MapToStringAsDouble(value_));

template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<bool>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<int32_t>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<uint32_t>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<int64_t>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<uint64_t>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<double>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::string>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<bool>>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<int32_t>>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<uint32_t>>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<int64_t>>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<uint64_t>>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<double>>;
template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<std::string>>;

}  // namespace webrtc
