/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_STATS_RTC_STATS_H_
#define API_STATS_RTC_STATS_H_

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/system/rtc_export_template.h"

namespace webrtc {

class RTCStatsMemberInterface;

// Abstract base class for RTCStats-derived dictionaries, see
// https://w3c.github.io/webrtc-stats/.
//
// All derived classes must have the following static variable defined:
//   static const char kType[];
// It is used as a unique class identifier and a string representation of the
// class type, see https://w3c.github.io/webrtc-stats/#rtcstatstype-str*.
// Use the `WEBRTC_RTCSTATS_IMPL` macro when implementing subclasses, see macro
// for details.
//
// Derived classes list their dictionary members, RTCStatsMember<T>, as public
// fields, allowing the following:
//
// RTCFooStats foo("fooId", GetCurrentTime());
// foo.bar = 42;
// foo.baz = std::vector<std::string>();
// foo.baz->push_back("hello world");
// uint32_t x = *foo.bar;
//
// Pointers to all the members are available with `Members`, allowing iteration:
//
// for (const RTCStatsMemberInterface* member : foo.Members()) {
//   printf("%s = %s\n", member->name(), member->ValueToString().c_str());
// }
class RTC_EXPORT RTCStats {
 public:
  RTCStats(const std::string& id, int64_t timestamp_us)
      : id_(id), timestamp_us_(timestamp_us) {}
  RTCStats(std::string&& id, int64_t timestamp_us)
      : id_(std::move(id)), timestamp_us_(timestamp_us) {}
  virtual ~RTCStats() {}

  virtual std::unique_ptr<RTCStats> copy() const = 0;

  const std::string& id() const { return id_; }
  // Time relative to the UNIX epoch (Jan 1, 1970, UTC), in microseconds.
  int64_t timestamp_us() const { return timestamp_us_; }
  // Returns the static member variable `kType` of the implementing class.
  virtual const char* type() const = 0;
  // Returns a vector of pointers to all the `RTCStatsMemberInterface` members
  // of this class. This allows for iteration of members. For a given class,
  // `Members` always returns the same members in the same order.
  std::vector<const RTCStatsMemberInterface*> Members() const;
  // Checks if the two stats objects are of the same type and have the same
  // member values. Timestamps are not compared. These operators are exposed for
  // testing.
  bool operator==(const RTCStats& other) const;
  bool operator!=(const RTCStats& other) const;

  // Creates a JSON readable string representation of the stats
  // object, listing all of its members (names and values).
  std::string ToJson() const;

  // Downcasts the stats object to an `RTCStats` subclass `T`. DCHECKs that the
  // object is of type `T`.
  template <typename T>
  const T& cast_to() const {
    RTC_DCHECK_EQ(type(), T::kType);
    return static_cast<const T&>(*this);
  }

 protected:
  // Gets a vector of all members of this `RTCStats` object, including members
  // derived from parent classes. `additional_capacity` is how many more members
  // shall be reserved in the vector (so that subclasses can allocate a vector
  // with room for both parent and child members without it having to resize).
  virtual std::vector<const RTCStatsMemberInterface*>
  MembersOfThisObjectAndAncestors(size_t additional_capacity) const;

  std::string const id_;
  int64_t timestamp_us_;
};

// All `RTCStats` classes should use these macros.
// `WEBRTC_RTCSTATS_DECL` is placed in a public section of the class definition.
// `WEBRTC_RTCSTATS_IMPL` is placed outside the class definition (in a .cc).
//
// These macros declare (in _DECL) and define (in _IMPL) the static `kType` and
// overrides methods as required by subclasses of `RTCStats`: `copy`, `type` and
// `MembersOfThisObjectAndAncestors`. The |...| argument is a list of addresses
// to each member defined in the implementing class. The list must have at least
// one member.
//
// (Since class names need to be known to implement these methods this cannot be
// part of the base `RTCStats`. While these methods could be implemented using
// templates, that would only work for immediate subclasses. Subclasses of
// subclasses also have to override these methods, resulting in boilerplate
// code. Using a macro avoids this and works for any `RTCStats` class, including
// grandchildren.)
//
// Sample usage:
//
// rtcfoostats.h:
//   class RTCFooStats : public RTCStats {
//    public:
//     WEBRTC_RTCSTATS_DECL();
//
//     RTCFooStats(const std::string& id, int64_t timestamp_us);
//
//     RTCStatsMember<int32_t> foo;
//     RTCStatsMember<int32_t> bar;
//   };
//
// rtcfoostats.cc:
//   WEBRTC_RTCSTATS_IMPL(RTCFooStats, RTCStats, "foo-stats"
//       &foo,
//       &bar);
//
//   RTCFooStats::RTCFooStats(const std::string& id, int64_t timestamp_us)
//       : RTCStats(id, timestamp_us),
//         foo("foo"),
//         bar("bar") {
//   }
//
#define WEBRTC_RTCSTATS_DECL()                                          \
 protected:                                                             \
  std::vector<const webrtc::RTCStatsMemberInterface*>                   \
  MembersOfThisObjectAndAncestors(size_t local_var_additional_capacity) \
      const override;                                                   \
                                                                        \
 public:                                                                \
  static const char kType[];                                            \
                                                                        \
  std::unique_ptr<webrtc::RTCStats> copy() const override;              \
  const char* type() const override

#define WEBRTC_RTCSTATS_IMPL(this_class, parent_class, type_str, ...)          \
  const char this_class::kType[] = type_str;                                   \
                                                                               \
  std::unique_ptr<webrtc::RTCStats> this_class::copy() const {                 \
    return std::make_unique<this_class>(*this);                                \
  }                                                                            \
                                                                               \
  const char* this_class::type() const { return this_class::kType; }           \
                                                                               \
  std::vector<const webrtc::RTCStatsMemberInterface*>                          \
  this_class::MembersOfThisObjectAndAncestors(                                 \
      size_t local_var_additional_capacity) const {                            \
    const webrtc::RTCStatsMemberInterface* local_var_members[] = {             \
        __VA_ARGS__};                                                          \
    size_t local_var_members_count =                                           \
        sizeof(local_var_members) / sizeof(local_var_members[0]);              \
    std::vector<const webrtc::RTCStatsMemberInterface*>                        \
        local_var_members_vec = parent_class::MembersOfThisObjectAndAncestors( \
            local_var_members_count + local_var_additional_capacity);          \
    RTC_DCHECK_GE(                                                             \
        local_var_members_vec.capacity() - local_var_members_vec.size(),       \
        local_var_members_count + local_var_additional_capacity);              \
    local_var_members_vec.insert(local_var_members_vec.end(),                  \
                                 &local_var_members[0],                        \
                                 &local_var_members[local_var_members_count]); \
    return local_var_members_vec;                                              \
  }

// A version of WEBRTC_RTCSTATS_IMPL() where "..." is omitted, used to avoid a
// compile error on windows. This is used if the stats dictionary does not
// declare any members of its own (but perhaps its parent dictionary does).
#define WEBRTC_RTCSTATS_IMPL_NO_MEMBERS(this_class, parent_class, type_str) \
  const char this_class::kType[] = type_str;                                \
                                                                            \
  std::unique_ptr<webrtc::RTCStats> this_class::copy() const {              \
    return std::make_unique<this_class>(*this);                             \
  }                                                                         \
                                                                            \
  const char* this_class::type() const { return this_class::kType; }        \
                                                                            \
  std::vector<const webrtc::RTCStatsMemberInterface*>                       \
  this_class::MembersOfThisObjectAndAncestors(                              \
      size_t local_var_additional_capacity) const {                         \
    return parent_class::MembersOfThisObjectAndAncestors(0);                \
  }

// Non-standard stats members can be exposed to the JavaScript API in Chrome
// e.g. through origin trials. The group ID can be used by the blink layer to
// determine if a stats member should be exposed or not. Multiple non-standard
// stats members can share the same group ID so that they are exposed together.
enum class NonStandardGroupId {
  // Group ID used for testing purposes only.
  kGroupIdForTesting,
  // I2E:
  // https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/hE2B1iItPDk
  kRtcAudioJitterBufferMaxPackets,
  // I2E:
  // https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/YbhMyqLXXXo
  kRtcStatsRelativePacketArrivalDelay,
};

// Certain stat members should only be exposed to the JavaScript API in
// certain circumstances as to avoid passive fingerprinting.
enum class StatExposureCriteria : uint8_t {
  // The stat should always be exposed. This is the default.
  kAlways,
  // The stat should only be exposed if the hardware capabilities described in
  // the stats spec are met. The requirements for this are described at
  // https://w3c.github.io/webrtc-stats/#limiting-exposure-of-hardware-capabilities.
  kHardware,
};

// Interface for `RTCStats` members, which have a name and a value of a type
// defined in a subclass. Only the types listed in `Type` are supported, these
// are implemented by `RTCStatsMember<T>`. The value of a member may be
// undefined, the value can only be read if `is_defined`.
class RTCStatsMemberInterface {
 public:
  // Member value types.
  enum Type {
    kBool,    // bool
    kInt32,   // int32_t
    kUint32,  // uint32_t
    kInt64,   // int64_t
    kUint64,  // uint64_t
    kDouble,  // double
    kString,  // std::string

    kSequenceBool,    // std::vector<bool>
    kSequenceInt32,   // std::vector<int32_t>
    kSequenceUint32,  // std::vector<uint32_t>
    kSequenceInt64,   // std::vector<int64_t>
    kSequenceUint64,  // std::vector<uint64_t>
    kSequenceDouble,  // std::vector<double>
    kSequenceString,  // std::vector<std::string>

    kMapStringUint64,  // std::map<std::string, uint64_t>
    kMapStringDouble,  // std::map<std::string, double>
  };

  virtual ~RTCStatsMemberInterface() {}

  const char* name() const { return name_; }
  virtual Type type() const = 0;
  virtual bool is_sequence() const = 0;
  virtual bool is_string() const = 0;
  bool is_defined() const { return is_defined_; }
  // Is this part of the stats spec? Used so that chromium can easily filter
  // out anything unstandardized.
  virtual bool is_standardized() const = 0;
  // Non-standard stats members can have group IDs in order to be exposed in
  // JavaScript through experiments. Standardized stats have no group IDs.
  virtual std::vector<NonStandardGroupId> group_ids() const { return {}; }
  // Should this stat be filtered out based on some criteria.
  virtual StatExposureCriteria exposure_criteria() const = 0;
  // Type and value comparator. The names are not compared. These operators are
  // exposed for testing.
  bool operator==(const RTCStatsMemberInterface& other) const {
    return IsEqual(other);
  }
  bool operator!=(const RTCStatsMemberInterface& other) const {
    return !(*this == other);
  }
  virtual std::string ValueToString() const = 0;
  // This is the same as ValueToString except for kInt64 and kUint64 types,
  // where the value is represented as a double instead of as an integer.
  // Since JSON stores numbers as floating point numbers, very large integers
  // cannot be accurately represented, so we prefer to display them as doubles
  // instead.
  virtual std::string ValueToJson() const = 0;

  template <typename T>
  const T& cast_to() const {
    RTC_DCHECK_EQ(type(), T::StaticType());
    return static_cast<const T&>(*this);
  }

 protected:
  RTCStatsMemberInterface(const char* name, bool is_defined)
      : name_(name), is_defined_(is_defined) {}

  virtual bool IsEqual(const RTCStatsMemberInterface& other) const = 0;

  const char* const name_;
  bool is_defined_;
};

namespace rtc_stats_internal {

typedef std::map<std::string, uint64_t> MapStringUint64;
typedef std::map<std::string, double> MapStringDouble;

// Produces "[a,b,c]". Works for non-vector `RTCStatsMemberInterface::Type`
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

// This overload is required because std::vector<bool> range loops don't
// return references but objects, causing -Wrange-loop-analysis diagnostics.
std::string VectorToString(const std::vector<bool>& vector);

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
std::string MapToString(const std::map<std::string, T>& map) {
  rtc::StringBuilder sb;
  sb << "{";
  const char* separator = "";
  for (const auto& element : map) {
    sb << separator << rtc::ToString(element.first) << ":"
       << rtc::ToString(element.second);
    separator = ",";
  }
  sb << "}";
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

template <typename T>
std::string MapToStringAsDouble(const std::map<std::string, T>& map) {
  rtc::StringBuilder sb;
  sb << "{";
  const char* separator = "";
  for (const auto& element : map) {
    sb << separator << "\"" << rtc::ToString(element.first)
       << "\":" << ToStringAsDouble(element.second);
    separator = ",";
  }
  sb << "}";
  return sb.Release();
}

// TODO(eshr): Replace the ValueToString and ValueToJson values by templated
// to string functions that can be called directly.
template <typename T>
struct stat_type_t {};

template <>
struct stat_type_t<bool> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kBool;
  static std::string ValueToString(bool value) { return rtc::ToString(value); }
  static std::string ValueToJson(bool value) { return rtc::ToString(value); }
};
template <>
struct stat_type_t<int32_t> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kInt32;
  static std::string ValueToString(int32_t value) {
    return rtc::ToString(value);
  }
  static std::string ValueToJson(int32_t value) { return rtc::ToString(value); }
};
template <>
struct stat_type_t<uint32_t> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kUint32;
  static std::string ValueToString(uint32_t value) {
    return rtc::ToString(value);
  }
  static std::string ValueToJson(uint32_t value) {
    return rtc::ToString(value);
  }
};
template <>
struct stat_type_t<int64_t> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kInt64;
  static std::string ValueToString(int64_t value) {
    return rtc::ToString(value);
  }
  static std::string ValueToJson(int64_t value) {
    return ToStringAsDouble(value);
  }
};

template <>
struct stat_type_t<uint64_t> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kUint64;
  static std::string ValueToString(int64_t value) {
    return rtc::ToString(value);
  }
  static std::string ValueToJson(int64_t value) {
    return ToStringAsDouble(value);
  }
};
template <>
struct stat_type_t<double> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kDouble;
  static std::string ValueToString(double value) {
    return rtc::ToString(value);
  }
  static std::string ValueToJson(double value) {
    return ToStringAsDouble(value);
  }
};
template <>
struct stat_type_t<std::string> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kString;
  static std::string ValueToString(std::string value) { return value; }
  static std::string ValueToJson(std::string value) { return value; }
};
template <>
struct stat_type_t<std::vector<bool>> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kSequenceBool;
  static std::string ValueToString(const std::vector<bool>& value) {
    return VectorToString(value);
  }
  static std::string ValueToJson(const std::vector<bool>& value) {
    return VectorToString(value);
  }
};
template <>
struct stat_type_t<std::vector<int32_t>> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kSequenceInt32;
  static std::string ValueToString(const std::vector<int32_t>& value) {
    return VectorToString(value);
  }
  static std::string ValueToJson(const std::vector<int32_t>& value) {
    return VectorToString(value);
  }
};
template <>
struct stat_type_t<std::vector<uint32_t>> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kSequenceUint32;
  static std::string ValueToString(const std::vector<uint32_t>& value) {
    return VectorToString(value);
  }
  static std::string ValueToJson(const std::vector<uint32_t>& value) {
    return VectorToString(value);
  }
};
template <>
struct stat_type_t<std::vector<int64_t>> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kSequenceInt64;
  static std::string ValueToString(const std::vector<int64_t>& value) {
    return VectorToString(value);
  }
  static std::string ValueToJson(const std::vector<int64_t>& value) {
    return VectorToStringAsDouble(value);
  }
};
template <>
struct stat_type_t<std::vector<uint64_t>> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kSequenceUint64;
  static std::string ValueToString(const std::vector<uint64_t>& value) {
    return VectorToString(value);
  }
  static std::string ValueToJson(const std::vector<uint64_t>& value) {
    return VectorToStringAsDouble(value);
  }
};
template <>
struct stat_type_t<std::vector<double>> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kSequenceDouble;
  static std::string ValueToString(const std::vector<double>& value) {
    return VectorToString(value);
  }
  static std::string ValueToJson(const std::vector<double>& value) {
    return VectorToStringAsDouble(value);
  }
};
template <>
struct stat_type_t<std::vector<std::string>> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kSequenceString;
  static std::string ValueToString(const std::vector<std::string>& value) {
    return VectorOfStringsToString(value);
  }
  static std::string ValueToJson(const std::vector<std::string>& value) {
    return VectorOfStringsToString(value);
  }
};
template <>
struct stat_type_t<MapStringUint64> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kMapStringUint64;
  static std::string ValueToString(const MapStringUint64& value) {
    return MapToString(value);
  }
  static std::string ValueToJson(const MapStringUint64& value) {
    return MapToStringAsDouble(value);
  }
};
template <>
struct stat_type_t<MapStringDouble> {
  static constexpr RTCStatsMemberInterface::Type type =
      RTCStatsMemberInterface::kMapStringDouble;
  static std::string ValueToString(const MapStringDouble& value) {
    return MapToString(value);
  }
  static std::string ValueToJson(const MapStringDouble& value) {
    return MapToStringAsDouble(value);
  }
};

template <typename T>
struct is_string_t : std::false_type {};
template <>
struct is_string_t<std::string> : std::true_type {};

template <typename T>
struct is_sequence_t : std::false_type {};
template <typename T>
struct is_sequence_t<std::vector<T>> : std::true_type {};

// template <typename T>
// inline std::string ValueToString(T t) {
//   return rtc::ToString(t);
// }
// template <typename T>
// inline std::string ValueToString(const std::vector<T>& v) {
//   return VectorToString(v);
// }
// template <>
// inline std::string ValueToString(const std::vector<std::string>& v) {
//   return VectorOfStringsToString(v);
// }
// template <typename T>
// inline std::string ValueToString(const std::map<std::string, T>& m) {
//   return MapToString(m);
// }
// template <>
// inline std::string ValueToString(std::string s) {
//   return s;
// }

// template <typename T>
// struct needs_to_string_as_double : std::false_type {};
// template <>
// struct needs_to_string_as_double<double> : std::true_type {};
// template <>
// struct needs_to_string_as_double<int64_t> : std::true_type {};
// template <>
// struct needs_to_string_as_double<uint64_t> : std::true_type {};

// template <typename T, typename = void>
// inline std::string PrimitiveValueToJson(T t) {
//   return rtc::ToString(t);
// }
// template <typename T,
//           std::enable_if_t<needs_to_string_as_double<T>::value> = true>
// inline std::string PrimitiveValueToJson(T t) {
//   return ToStringAsDouble(t);
// }

// template <typename T>
// inline std::string ValueToJson(T t) {
//   return PrimitiveValueToJson(t);
// }
// template <typename T, typename = void>
// inline std::string ValueToJson(const std::vector<T>& v) {
//   return ValueToString(v);
// }
// template <typename T, std::enable_if_t<needs_to_string_as_double<T>::value>>
// inline std::string ValueToJson(const std::vector<T>& v) {
//   return VectorToStringAsDouble(v);
// }
// template <typename T>
// inline std::string ValueToJson(const std::map<std::string, T>& m) {
//   return MapToStringAsDouble(m);
// }
// template <>
// inline std::string ValueToJson(std::string s) {
//   return s;
// }

}  // namespace rtc_stats_internal

// Template implementation of `RTCStatsMemberInterface`.
// The supported types are the ones described by
// `RTCStatsMemberInterface::Type`.
template <typename T, StatExposureCriteria E = StatExposureCriteria::kAlways>
class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT) RTCStatsMember
    : public RTCStatsMemberInterface {
 public:
  explicit RTCStatsMember(const char* name)
      : RTCStatsMemberInterface(name, /*is_defined=*/false), value_() {}
  RTCStatsMember(const char* name, const T& value)
      : RTCStatsMemberInterface(name, /*is_defined=*/true), value_(value) {}
  RTCStatsMember(const char* name, T&& value)
      : RTCStatsMemberInterface(name, /*is_defined=*/true),
        value_(std::move(value)) {}
  RTCStatsMember(const RTCStatsMember<T, E>& other)
      : RTCStatsMemberInterface(other.name_, other.is_defined_),
        value_(other.value_) {}
  RTCStatsMember(RTCStatsMember<T, E>&& other)
      : RTCStatsMemberInterface(other.name_, other.is_defined_),
        value_(std::move(other.value_)) {}

  static Type StaticType() { return stat_type::type; }
  RTC_EXPORT Type type() const override { return StaticType(); }
  RTC_EXPORT bool is_sequence() const override {
    return rtc_stats_internal::is_sequence_t<T>::value;
  }
  RTC_EXPORT bool is_string() const override {
    return rtc_stats_internal::is_string_t<T>::value;
  }
  StatExposureCriteria exposure_criteria() const override { return E; }
  bool is_standardized() const override { return true; }
  RTC_EXPORT std::string ValueToString() const override {
    return stat_type::ValueToString(value_);
    // return rtc_stats_internal::ValueToString(value_);
  }
  RTC_EXPORT std::string ValueToJson() const override {
    return stat_type::ValueToJson(value_);
    // return rtc_stats_internal::ValueToJson(value_);
  }

  template <typename U>
  inline T ValueOrDefault(U default_value) const {
    if (is_defined()) {
      return *(*this);
    }
    return default_value;
  }

  // Assignment operators.
  T& operator=(const T& value) {
    value_ = value;
    is_defined_ = true;
    return value_;
  }
  T& operator=(const T&& value) {
    value_ = std::move(value);
    is_defined_ = true;
    return value_;
  }

  // Value getters.
  T& operator*() {
    RTC_DCHECK(is_defined_);
    return value_;
  }
  const T& operator*() const {
    RTC_DCHECK(is_defined_);
    return value_;
  }

  // Value getters, arrow operator.
  T* operator->() {
    RTC_DCHECK(is_defined_);
    return &value_;
  }
  const T* operator->() const {
    RTC_DCHECK(is_defined_);
    return &value_;
  }

 protected:
  bool IsEqual(const RTCStatsMemberInterface& other) const override {
    if (type() != other.type() || is_standardized() != other.is_standardized())
      return false;
    const RTCStatsMember<T, E>& other_t =
        static_cast<const RTCStatsMember<T, E>&>(other);
    if (!is_defined_)
      return !other_t.is_defined();
    if (!other.is_defined())
      return false;
    return value_ == other_t.value_;
  }

 private:
  using stat_type = rtc_stats_internal::stat_type_t<T>;
  T value_;
};

// Using inheritance just so that it's obvious from the member's declaration
// whether it's standardized or not.
template <typename T>
class RTCNonStandardStatsMember : public RTCStatsMember<T> {
 public:
  explicit RTCNonStandardStatsMember(const char* name)
      : RTCStatsMember<T>(name) {}
  RTCNonStandardStatsMember(const char* name,
                            std::initializer_list<NonStandardGroupId> group_ids)
      : RTCStatsMember<T>(name), group_ids_(group_ids) {}
  RTCNonStandardStatsMember(const char* name, const T& value)
      : RTCStatsMember<T>(name, value) {}
  RTCNonStandardStatsMember(const char* name, T&& value)
      : RTCStatsMember<T>(name, std::move(value)) {}
  RTCNonStandardStatsMember(const RTCNonStandardStatsMember<T>& other)
      : RTCStatsMember<T>(other), group_ids_(other.group_ids_) {}
  RTCNonStandardStatsMember(RTCNonStandardStatsMember<T>&& other)
      : RTCStatsMember<T>(std::move(other)),
        group_ids_(std::move(other.group_ids_)) {}

  bool is_standardized() const override { return false; }

  std::vector<NonStandardGroupId> group_ids() const override {
    return group_ids_;
  }

  T& operator=(const T& value) { return RTCStatsMember<T>::operator=(value); }
  T& operator=(const T&& value) {
    return RTCStatsMember<T>::operator=(std::move(value));
  }

 private:
  std::vector<NonStandardGroupId> group_ids_;
};

extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<bool>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<int32_t>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<uint32_t>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<int64_t>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<uint64_t>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<double>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::string>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<bool>>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<int32_t>>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<uint32_t>>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<int64_t>>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<uint64_t>>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<double>>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::vector<std::string>>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::map<std::string, uint64_t>>;
extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)
    RTCNonStandardStatsMember<std::map<std::string, double>>;

}  // namespace webrtc

#endif  // API_STATS_RTC_STATS_H_
