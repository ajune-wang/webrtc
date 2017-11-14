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
// A StructuredForm is a stump if its value is a string, or otherwise it has
// children StructuredForm's in its value.
//
// The current implementation utilizes JSON objects (implemented by jsoncpp) as
// the infrastructure
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
  StructuredForm(const StructuredForm& other) { Init(other); }

  virtual StructuredForm& operator=(const StructuredForm& other) {
    Init(other);
    return *this;
  }

  virtual bool operator==(const StructuredForm& other) const {
    return (key_ == other.key_) &&
           /*jsoncpp implementation specific*/ (value_ == other.value_);
  }

  virtual bool operator!=(const StructuredForm& other) const {
    return !operator==(other);
  }

  // The following methods implement data operations on a StructuredForm using
  // the underlying data representation infrastructure (jsoncpp currently) and
  // maintain the conceptual structure of a StructuredForm described above.

  // Replaces any existing value with the given string. The
  // original structured form before value setting is returned.
  StructuredForm SetValueAsString(const std::string& value_str);

  // Replaces any existing value with the given StructuredForm.
  // The original structured form before value setting is return.
  StructuredForm SetValueAsStructuredForm(const StructuredForm& child);

  // Returns true if a child StructuredForm with the given key exists in the
  // value and false otherwise
  bool HasChildWithKey(const std::string child_key) const;

  // Returns false if the parent StructuredForm is a stump, or otherwise
  // adds a StructuredForm to the set of StructuredForms in the value, replaces
  // any existing child with the same key and returns true.
  bool AddChild(const StructuredForm& child);

  // Returns false if there is no existing child with the same key, or otherwise
  // replaces the existing child with the same key and return true.
  bool UpdateChild(const StructuredForm& child);

  // Returns a copy of a child StructuredForm with the given key in the value if
  // it exists and otherwise a kNullStructuredForm is returned.
  StructuredForm GetChildWithKey(const std::string& child_key) const;

  bool IsStump() const;
  bool IsNull() const;

  std::string key() const { return key_; }
  void set_key(const std::string& key) { key_ = key; }

  // The keys of children can be used to iterate child StructuredForm's but a
  // better approach is to define a new iterator instead of
  // implementation-specific iteration using string keys
  std::unordered_set<std::string> child_keys() const { return child_keys_; }

  // from Describable
  /*virtual*/ std::string ToString() const override;
  /*virtual*/ StructuredForm ToStructuredForm() const override { return *this; }

  Json::Value as_json() const;

 protected:
  std::string key_;
  // internal implementation using jsoncpp
  Json::Value value_;

  std::unordered_set<std::string> child_keys_;

 private:
  void Init(const StructuredForm& other);

  // if not Json::Value::null, value_ can only be either a string or a JSON
  // object; otherwise, this is an implementation bug
  void SanityCheckWhenUsingJsonImplementation() const;

  // internal use to construct a StructuredForm with jsoncpp
  StructuredForm(const std::string& key, Json::Value value);
};

const StructuredForm kNullStructuredForm("null");

// When defining objects on top of the StructuredForm, customized internal data
// should be defined using the following macro and data mutation should be
// done via the setter. The internal data is likely not boxed in the value of
// the underlying StructuredForm, which can cause information missing in
// serialization and data transfer using the infrastructure provided by the
// StructuredForm. To box and synchronize the internal data in the
// StructuredForm in a correct way, use the HasUnboxedInternalData interface
// defined below to reinforce boxing in construction and implement the setters
// defined here to sync the data with boxed version.
#define ADD_UNBOXED_DATA_WITH_UNDEFINED_SETTER(type, name) \
 private:                                                  \
  type name##_;                                            \
                                                           \
 public:                                                   \
  void set_##name(const type&);

#define ADD_UNBOXED_DATA_WITH_SIMPLE_SETTER(type, name) \
 private:                                               \
  type name##_;                                         \
                                                        \
 public:                                                \
  void set_##name(type const value) { name##_ = value; }

// The HasUnboxedInternalData interface is to reinforce the structure of a
// LogObject (defined below) as a StructuredForm when it has customized data
// that are statically defined in the compile time via member declaration, e.g.
// the creation timestamp of a log event or the starting timestamp of the valid
// period of an event hook have their own internal data. The intention is to
// remind a user to box the necessary internal data in constructor to the
// underlying StructuredForm for easy data representation and transferring (as a
// Describable).
//
// In addition, each LogObject can have a child StructuredForm with key "data"
// to hold any customized data defined in the runtime by calling the method
// AddData (see definition of LogObject).
template <bool has_unboxed_internal_data>
struct HasUnboxedInternalData;

template <>
struct HasUnboxedInternalData<true> {
  virtual void BoxInternalDataInConstructor() = 0;
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

  /*virtual*/ LogObject& operator=(const LogObject& other) {
    StructuredForm::operator=(other);
    return *this;
  }

  // Creates a child StructuredForm with key "data" if it does not exists to
  // hold internal data defined in runtime, and then appends a grandchild -
  // created with the given key with the given value - to the data child. If
  // reduce_level is true, the key of the StructuredForm holding the value is
  // replaced by the given key to reduce the level of hierarchy. E.g, using the
  // JSON notation, after adding a piece of data {'timestamp': 1} with the key
  // 'Timestamp' to a LogObject, the LogObject is represented as
  //  1) if reduce_level = true,
  //     {'key_of_the_object':
  //       [ // set of child StructuredForm
  //         {'data':
  //           {'Timestamp':
  //             {'timestamp': 1}
  //           },
  //           // ...other dynamically defined data grandchild StructuredForm's
  //         },
  //         // ...other statically defined child StructuredForm's
  //       ]
  //     }
  //  2) if reduced_level = false,
  //     {'key_of_the_object':
  //       [ // set of child StructuredForm
  //         {'data':
  //           {'Timestamp': 1}
  //           // ...other dynamically defined data grandchild StructuredForm's
  //         },
  //         // ...other statically defined child StructuredForm's
  //       ]
  //     }
  void AddData(const std::string data_key,
               const StructuredForm& data_value,
               bool reduce_level = false) {
    StructuredForm data_grandchild(data_key);
    if (reduce_level) {
      data_grandchild = data_value;
      data_grandchild.set_key(data_key);
    } else {
      data_grandchild.SetValueAsStructuredForm(data_value);
    }

    StructuredForm data_child = GetChildWithKey("data");
    if (data_child.IsNull()) {
      // first time adding dynamic data
      data_child = StructuredForm("data");
      RTC_DCHECK(AddChild(data_child));
    }
    RTC_DCHECK(data_child.AddChild(data_grandchild));
    RTC_DCHECK(UpdateChild(data_child));
  }

  virtual ~LogObject() {}
};

// DEFINE_ENUMERATED_ICE_OBJECT defines an LogObject-derived type with a
// build-in stringified enum type (see stringifiedenum.h). "enum_name"
// will be the type name and enum_key will be the key for the underlying
// LogObject when it is treated as a key-value pair based StructuredForm.
// Refer to the definition of StructuredForm below.

#define DECLARE_ENUMERATED_ICE_OBJECT(enum_name, enum_key, ...)       \
  DECLARE_STRINGIFIED_ENUM(enum_name##Base, __VA_ARGS__);             \
  class enum_name : public enum_name##Base, public LogObject<false> { \
   public:                                                            \
    enum_name() {}                                                    \
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

// LogEventType
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

// LogEvent and LogHook
class LogHook;
using Timestamp = uint64_t;

class LogEvent final : public LogObject<true> {
 public:
  explicit LogEvent(const LogEventType& type);
  explicit LogEvent(const LogEvent& other);

  /*virtual*/ LogEvent& operator=(const LogEvent& other);

  // from HasUnboxedInternalData
  /*virtual*/ void BoxInternalDataInConstructor() override;

  void AddHookForDownstreamEvents(const LogHook& hook);
  void UpdateUpstreamEvents();
  // similar to AddData
  void AddSignature(const std::string& signature_key,
                    const StructuredForm& signature_value);

  std::string id() const { return id_; }
  LogEventType::Value type() const { return type_.value(); }
  Timestamp created_at() const { return event_created_at_; }
  std::unordered_set<LogEvent*> upstream_events() const {
    return upstream_events_;
  }

  ~LogEvent() {}

  // internal customized data
  ADD_UNBOXED_DATA_WITH_SIMPLE_SETTER(std::string, id);
  ADD_UNBOXED_DATA_WITH_SIMPLE_SETTER(Timestamp, event_created_at);
  ADD_UNBOXED_DATA_WITH_SIMPLE_SETTER(LogEventType, type);
  ADD_UNBOXED_DATA_WITH_UNDEFINED_SETTER(std::unordered_set<LogEvent*>,
                                         upstream_events);

 private:
  void Init(const LogEvent& other);
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

class LogHook final : public LogObject<true> {
 public:
  LogHook() = delete;
  explicit LogHook(LogEvent* originating_event,
                   LogEventType::Value downstream_event_type);

  // from HasUnboxedInternalData
  /*virtual*/ void BoxInternalDataInConstructor() override;

  void AddConstraintForDownstreamEvent(const std::string& constraint_key,
                                       const StructuredForm& constraint_value);
  bool CanBeAttachedByDownstreamEvent(const LogEvent& event) const;

  Timestamp valid_from() const { return hook_valid_from_; }
  LogEvent* originating_event() const { return originating_event_; }
  LogEventType::Value downstream_event_type() const {
    return downstream_event_type_;
  }

  ~LogHook() {}

  // internal customized data
  ADD_UNBOXED_DATA_WITH_SIMPLE_SETTER(Timestamp, hook_valid_from);
  ADD_UNBOXED_DATA_WITH_SIMPLE_SETTER(LogEvent*, originating_event);
  ADD_UNBOXED_DATA_WITH_SIMPLE_SETTER(LogEventType::Value,
                                      downstream_event_type);

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

  const IceCandidateProperty* local_candidate_property() {
    return local_cand_property_;
  }
  const IceCandidateProperty* remote_candidate_property() {
    return remote_cand_property_;
  }

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
