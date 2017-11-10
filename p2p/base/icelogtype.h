/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICELOGTYPE_H_
#define P2P_BASE_ICELOGTYPE_H_

#include <deque>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "p2p/base/candidate.h"
#include "p2p/base/port.h"
#include "p2p/base/stringifiedenum.h"

#ifndef WEBRTC_EXTERNAL_JSON
#define WEBRTC_EXTERNAL_JSON
#include "rtc_base/json.h"
#undef WEBRTC_EXTERNAL_JSON
#else
#include "rtc_base/json.h"
#endif

namespace webrtc {

namespace icelog {

// DEFINE_ENUMERATED_ICE_OBJECT defines an LogObject-derived type with a
// build-in stringified enum type (see stringifiedenum.h). "enum_name"
// will be the type name and enum_key will be the key for the underlying
// LogObject when it is treated as a key-value pair based StructuredForm.
// Refer to the definition of StructuredForm below.

#define DECLARE_ENUMERATED_ICE_OBJECT(enum_name, enum_key, ...)       \
  DECLARE_STRINGIFIED_ENUM(enum_name##Base, __VA_ARGS__);             \
  class enum_name : public enum_name##Base, public LogObject<false> { \
   public:                                                            \
    explicit enum_name(Value enum_val)                                \
        : enum_name##Base(enum_val), LogObject(#enum_key) {           \
      std::string enum_val_str = EnumToStr(enum_val);                 \
      if (enum_val == enum_name::kUndefined) {                        \
        LogObject::value_["value"] = "undefined";                     \
        LogObject::value_["comment"] = enum_val_str;                  \
        return;                                                       \
      }                                                               \
      LogObject::value_ = enum_val_str;                               \
    }                                                                 \
    enum_name(const enum_name& other)                                 \
        : enum_name##Base(other), LogObject(other) {}                 \
    ~enum_name() {}                                                   \
  };

#define DEFINE_ENUMERATED_ICE_OBJECT(enum_name, enum_key, ...) \
  DEFINE_STRINGIFIED_ENUM(enum_name##Base, __VA_ARGS__)

// The Describable interface prescribes the available conversion of data
// representation of an object for data transfer and human comprehension.
class StructuredForm;

class Describable {
 public:
  Describable() {}
  virtual std::string ToString() const = 0;
  virtual StructuredForm ToStructuredForm() const = 0;
  virtual ~Describable() {}
};

// StructuredForm is an abstraction on top of the underlying structured format
// like JSON or Protobuf.
// A StructuredForm is defined as a key-value pair, where
//  1) the key is a string and
//  2) the value is a string or a set of StructuredForm's
// The current implementation utilizes JSON objects as the infrastructure
class StructuredForm : public Describable {
 public:
  StructuredForm() : key_(""), value_("") {}
  // The value_str can be interpreted as a serialized StructuredForm and if
  // the value_str is not literal and the value is set using the parsed result.
  // The current implementation utilizes JSON objects as the infrastructure and
  // the JSON parser to parse nonliteral value_str
  StructuredForm(const std::string key,
                 const std::string value_str = std::string(),
                 bool is_value_str_literal = true);
  StructuredForm(const StructuredForm& other) { operator=(other); }

  StructuredForm& operator=(const StructuredForm& other) {
    key_ = other.key_;
    value_ = other.value_;
    return *this;
  }

  // The following methods implement data operations on a StructuredForm
  // and maintain the conceptual structure described above.

  // replaces any existing value with the given string. The
  // original structured form before value setting is return
  StructuredForm SetValueAsString(const std::string& value_str);

  // replaces any existing value with the given StructuredForm.
  // The original structured form before value setting is return
  StructuredForm SetValueAsStructuredForm(const StructuredForm& child);

  // inserts a StructuredForm to the set of StructuredForms in the
  // value and returns a nullStructuredForm, or it creates a new set containing
  // only the child if the original value is a string, and returns the original
  // StrucutredForm
  StructuredForm InsertStructuredFormToValue(const StructuredForm& child);

  // return the StructuredForm in the value with the given key if it exists and
  // otherwise a kNullStructuredForm is returned
  StructuredForm GetStructuredFormFromValue(const std::string& key_child) const;

  Json::Value as_json() const;
  std::string key() const { return key_; }
  void set_key(const std::string& key) { key_ = key; }
  bool isStump() const;

  // from Describable
  /*virtual*/ std::string ToString() const override;
  /*virtual*/ StructuredForm ToStructuredForm() const override { return *this; }

 protected:
  std::string key_;
  // internal implementation using jsoncpp
  Json::Value value_;

 private:
  // if not Json::Value::null, value_ can only be either a string or a JSON
  // object; otherwise, this is an implementation bug
  void SanityCheckWhenUsingJsonImplementation() const;
  // internal use to construct a StructuredForm with jsoncpp
  StructuredForm(const std::string& key, Json::Value value) {
    key_ = key;
    value_ = value;
  }
};

// This interface is to reinforce the structure of a log object (defined below)
// as a StructuredForm when it has customized data - e.g. a log event and an
// event hook have their own internal data. The intention is to remind a user
// to box the necessary internal data to the underlying StructuredForm for easy
// data representation and transferring (as a Describable)
template <bool has_unboxed_internal_data>
struct HasUnboxedInternalData;

template <>
struct HasUnboxedInternalData<true> {
  virtual void BoxInternalData() = 0;
  virtual ~HasUnboxedInternalData() {}
};

template <>
struct HasUnboxedInternalData<false> {};

template <bool has_unboxed_internal_data = true>
class LogObject : public StructuredForm,
                  public HasUnboxedInternalData<has_unboxed_internal_data> {
 public:
  LogObject() {}
  explicit LogObject(const std::string key) : StructuredForm(key) {}
  LogObject(const LogObject& other) : StructuredForm(other) {}

  virtual LogObject& operator=(const LogObject& other) {
    StructuredForm::operator=(other);
    return *this;
  }

  // from StructuredForm as Describable
  // /*virtual*/ std::string ToString() const override;

  virtual ~LogObject() {}
};

// template<>
// std::string LogObject<true>::ToString() const {
//   Log
//   return "";
// }

// template<>
// std::string LogObject<true>::ToString() const {
//   return StructuredForm::ToString();
// }

// LogEvent and LogHook

DECLARE_ENUMERATED_ICE_OBJECT(LogEventType,
                              type,
                              kNone,
                              kAny,
                              kCandidateGathered,
                              kConnectionCreated,
                              kStunBindRequestSent,
                              kStunBindRequestResponseReceived,
                              kConnectionReselected,
                              kNumLogEventTypes);
class LogHook;
using Timestamp = uint64_t;

class LogEvent : public LogObject<true> {
 public:
  explicit LogEvent(const LogEventType& type);
  explicit LogEvent(const LogEvent& other);

  LogEvent& operator=(const LogEvent& other);

  // from HasUnboxedInternalData
  /*virtual*/ void BoxInternalData() override;

  void AddHookForDownstreamEvents(const LogHook& hook);
  void UpdateUpstreamEvents();

  // insert a k-v pair to underlying value of the event as a structured form
  void AddData(const std::string key,
               const StructuredForm& data_as_value,
               bool reduce_level = false);

  std::string id() const { return id_; }
  LogEventType::Value type() const { return type_->value(); }
  Timestamp created_at() const { return event_created_at_; }
  std::unordered_set<LogEvent*> upstream_events() const {
    return upstream_events_;
  }

  ~LogEvent();

 private:
  std::string id_;
  Timestamp event_created_at_;
  std::unique_ptr<LogEventType> type_;
  std::unordered_set<LogEvent*> upstream_events_;
};

class LogEventPool {
 public:
  // Singleton, constructor and destructor private
  static LogEventPool* Instance();
  // store the event and return a pointer to the stored copy
  LogEvent* RegisterEvent(const LogEvent& event);

 private:
  LogEventPool();
  ~LogEventPool();
  std::deque<LogEvent> internal_event_pool_;
};

class LogHook : public LogObject<true> {
 public:
  LogHook() = delete;
  explicit LogHook(LogEvent* originating_event,
                   LogEventType::Value downstream_event_type);

  // from HasUnboxedInternalData
  /*virtual*/ void BoxInternalData() override;

  void SetDownstreamEventType(LogEventType::Value type);
  // constraint must be a stump structured form
  void AddConstraintForDownstreamEvent(const StructuredForm& constraint);
  bool CanBeAttachedByADownstreamEvent(const LogEvent& event) const;

  Timestamp valid_from() const { return hook_valid_from_; }
  LogEvent* originating_event() const { return originating_event_; }
  LogEventType::Value downstream_event_type() const {
    return downstream_event_type_;
  }

 private:
  Timestamp hook_valid_from_;
  LogEvent* originating_event_;
  LogEventType::Value downstream_event_type_;

 public:
  size_t HashCode() const;
  // customized hasher
  class Hasher {
   public:
    size_t operator()(const LogHook& hook) const;
  };
  // customized comparator
  // The event hook is not defined as comparable since we only define so far a
  // binary predicate of the equality between two hooks.
  class Comparator {
   public:
    bool operator()(const LogHook& lhs, const LogHook& rhs) const;
  };
};

// hook pool handling event-hook attachment

class LogHookPool {
 public:
  // Singleton, constructor and destructor private
  static LogHookPool* Instance();

  void RegisterEventHook(const LogHook& hook);
  std::unordered_set<LogEvent*> GetUpstreamEventsForAnEvent(
      const LogEvent& event) const;

 private:
  LogHookPool();
  ~LogHookPool();
  std::unordered_set<LogHook, LogHook::Hasher, LogHook::Comparator>
      internal_hook_pool_;
};

enum class CompareResult { kLess, kGreater, kEqual };

template <typename T>
class Comparable {
 public:
  virtual CompareResult Compare(const T& other) const = 0;
  virtual bool operator<(const T& other) const {
    return Compare(other) == CompareResult::kLess;
  }
  virtual bool operator>(const T& other) const {
    return Compare(other) == CompareResult::kGreater;
  }
  virtual bool operator==(const T& other) const {
    return Compare(other) == CompareResult::kEqual;
  }
  virtual bool operator!=(const T& other) const {
    return Compare(other) != CompareResult::kEqual;
  }
  virtual ~Comparable() {}
};

// base class for identifiers
class LogIdentifier : public LogObject<false>,
                      public Comparable<LogIdentifier> {
 public:
  LogIdentifier();
  explicit LogIdentifier(const std::string& id);
  LogIdentifier(const LogIdentifier& other);

  CompareResult Compare(const LogIdentifier& other) const override;

  virtual std::string id() const { return id_; }

  virtual ~LogIdentifier() {}

 protected:
  // using id_ to avoid excessive boxing and unboxing when fetching
  // the id string
  std::string id_;
  // set the plain string id and also box it in value_
  void set_id(const std::string& id);
};

// dedicated identifier type for candidates
class IceCandidateId final : public LogIdentifier,
                             public Comparable<IceCandidateId> {
 public:
  IceCandidateId();
  explicit IceCandidateId(const std::string& id);

  using LogIdentifier::Compare;
  CompareResult Compare(const IceCandidateId& other) const override;
};

// dedicated identifier type for connections
class IceConnectionId final : public LogIdentifier,
                              public Comparable<IceConnectionId> {
 public:
  IceConnectionId();
  explicit IceConnectionId(const std::string& id);
  explicit IceConnectionId(cricket::Connection* conn);
  IceConnectionId(const IceCandidateId& local_cand_id,
                  const IceCandidateId& remote_cand_id);

  using LogIdentifier::Compare;
  CompareResult Compare(const IceConnectionId& other) const override;
};

// stringified enum classes
DECLARE_ENUMERATED_ICE_OBJECT(IceCandidateContent,
                              content,
                              kAudio,
                              kVideo,
                              kData);
DECLARE_ENUMERATED_ICE_OBJECT(IceCandidateProtocol,
                              protocol,
                              kUdp,
                              kTcp,
                              kSsltcp,
                              kTls);
DECLARE_ENUMERATED_ICE_OBJECT(IceCandidateType,
                              type,
                              kLocal,
                              kStun,
                              kPrflx,
                              kRelay);
DECLARE_ENUMERATED_ICE_OBJECT(IceCandidateNetwork, network, kWlan, kCell);

// the list of candidate properties can grow
class IceCandidateProperty final : public LogObject<false>,
                                   public Comparable<IceCandidateProperty> {
 public:
  IceCandidateProperty();
  IceCandidateProperty(const cricket::Port& port, const cricket::Candidate& c);

  CompareResult Compare(const IceCandidateProperty& other) const override;

  const IceCandidateId& id() const { return *id_; }
  std::string ip_addr() const { return ip_addr_; }
  bool is_remote() const { return is_remote_; }
  void set_is_remote(bool is_remote) { is_remote_ = is_remote; }

  ~IceCandidateProperty();

 private:
  std::unique_ptr<IceCandidateId> id_;
  std::unique_ptr<IceCandidateType> type_;
  std::unique_ptr<IceCandidateContent> content_;
  std::unique_ptr<IceCandidateProtocol> protocol_;
  std::unique_ptr<IceCandidateNetwork> network_;
  std::string ip_addr_;
  bool is_remote_;
};

// the connection property consists of the properties of the local and
// the remote candidates
class IceConnectionProperty final : public LogObject<false>,
                                    public Comparable<IceConnectionProperty> {
 public:
  IceConnectionProperty();
  IceConnectionProperty(const IceCandidateProperty& local_cand_property,
                        const IceCandidateProperty& remote_cand_property);

  CompareResult Compare(const IceConnectionProperty& other) const override;

 private:
  std::unique_ptr<IceConnectionId> id_;
  const IceCandidateProperty* local_cand_property_;
  const IceCandidateProperty* remote_cand_property_;
  StructuredForm as_json_;
  // bool is_selected;
};

// structured log messages for general purpose data recording and text logs
class LogMessage : public LogObject<false> {
 public:
  LogMessage();
  LogMessage& SetDescription(const std::string& type);
  LogMessage& SetData(const std::string& data_str);
  LogMessage& SetData(const StructuredForm& data);
  LogMessage& SetData(const std::vector<StructuredForm>& data_list);

 private:
  void Init();
};

}  // namespace icelog

}  // namespace webrtc

#endif  // P2P_BASE_ICELOGTYPE_H_
