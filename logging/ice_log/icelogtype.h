/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_ICE_LOG_ICELOGTYPE_H_
#define LOGGING_ICE_LOG_ICELOGTYPE_H_

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "logging/ice_log/stringifiedenum.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace icelog {

// The Describable interface prescribes the available conversion of data
// representation of an object for data transfer and human readability.
class StructuredForm;

class Describable {
 public:
  Describable() {}
  virtual std::string ToString() const = 0;
  virtual StructuredForm ToStructuredForm() const = 0;
  virtual ~Describable() {}
};

// StructuredForm is an abstraction on top of the underlying structured format.
// Currently a native implementation is provided but it can be used as an API
// layer for logging.
//
// A StructuredForm is defined as a key-value pair, where
//  1) the key is a string and
//  2) the value is a string or a set of StructuredForm's
// A StructuredForm is a stump if its value is a string, or otherwise it has
// children StructuredForm's in its value.
class StructuredForm : public Describable {
 public:
  StructuredForm();
  explicit StructuredForm(const std::string key);
  StructuredForm(const StructuredForm& other);

  virtual StructuredForm& operator=(const StructuredForm& other);

  virtual bool operator==(const StructuredForm& other) const;
  virtual bool operator!=(const StructuredForm& other) const;

  // The following methods implement data operations on a StructuredForm using
  // the underlying data representation infrastructure (jsoncpp currently) and
  // maintain the conceptual structure of a StructuredForm described above.

  // Replaces any existing value with the given string. The
  // original structured form before value setting is returned.
  StructuredForm SetValueAsString(const std::string& value_str);

  // Replaces any existing value with the given StructuredForm.
  // The original structured form before value setting is return.
  StructuredForm SetValueAsStructuredForm(const StructuredForm& child);

  StructuredForm SetValue(const std::string& value) {
    return SetValueAsString(value);
  }

  StructuredForm SetValue(const StructuredForm& value) {
    return SetValueAsStructuredForm(value);
  }

  // Returns true if a child StructuredForm with the given key exists in the
  // value and false otherwise
  bool HasChildWithKey(const std::string child_key) const;

  // Add a child StructuredForm if there is no existing child with the same key
  // and clear any string value if the parent is a stump, or otherwise replaces
  // the value of the child with same key.
  // Check using IsStump() if the string value should not be cleared in case the
  // parent is a stump.
  void AddChild(const StructuredForm& child);

  // Returns false if there is no existing child with the same key, or otherwise
  // replaces the existing child with the same key and return true.
  bool UpdateChild(const StructuredForm& child);

  // Returns a copy of a child StructuredForm with the given key in the value if
  // it exists and otherwise a kNullStructuredForm is returned.
  StructuredForm* GetChildWithKey(const std::string& child_key) const;

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
  /*virtual*/ StructuredForm ToStructuredForm() const override;

  void Serialize() const;

  ~StructuredForm() override;

 protected:
  std::string key_;
  std::string value_str_;
  std::unordered_map<std::string /*child_key*/,
                     std::unique_ptr<StructuredForm> /*child_sf*/>
      value_sf_set_;
  std::unordered_set<std::string> child_keys_;

 private:
  bool is_stump_;

 private:
  void Init(const StructuredForm& other);

  // if not Json::Value::null, value_ can only be either a string or a JSON
  // object; otherwise, this is an implementation bug
  // void SanityCheckWhenUsingJsonImplementation() const;

  // internal use to construct a StructuredForm with jsoncpp
  // StructuredForm(const std::string& key, Json::Value value);
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
#define ADD_UNBOXED_DATA_WITHOUT_SETTER(type, name) \
 private:                                           \
  type name##_;

#define ADD_UNBOXED_DATA_AND_DECLARE_SETTER(type, name) \
 private:                                               \
  type name##_;                                         \
                                                        \
 public:                                                \
  void set_##name(type const& value);

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

  // Creates a child StructuredForm with key data_section_key if it does not
  // exists to hold internal data defined in runtime, and then appends a
  // grandchild - created with the given key with the given string value - to
  // the data child.
  void AddData(const std::string& data_key,
               const std::string& data_value,
               bool, /*not used for string data value*/
               const std::string& data_section_key = "data") {
    AddDataImpl<std::string>(data_key, data_value, false, data_section_key);
  }

  // A similar method as above for the case when the data value is a
  // StructuredForm. If reduce_level is true, the key of the StructuredForm
  // holding the value is replaced by the given key to reduce the level of
  // hierarchy. E.g, using the JSON notation, after adding a piece of data
  // {'timestamp': 1} with the data_key 'Timestamp' to a LogObject, the
  // LogObject is represented as
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
  //  2) if reduce_level = false,
  //     {'key_of_the_object':
  //       [ // set of child StructuredForm
  //         {'data':
  //           {'Timestamp': 1}
  //           // ...other dynamically defined data grandchild StructuredForm's
  //         },
  //         // ...other statically defined child StructuredForm's
  //       ]
  //     }
  void AddData(const std::string& data_key,
               const StructuredForm& data_value,
               bool reduce_level = false,
               const std::string& data_section_key = "data") {
    AddDataImpl<StructuredForm>(data_key, data_value, reduce_level,
                                data_section_key);
  }

  virtual ~LogObject() {}

 private:
  template <typename data_value_type>
  void AddDataImpl(const std::string& data_key,
                   const data_value_type& data_value,
                   bool reduce_level = false,
                   const std::string& data_section_key = "data") {
    StructuredForm data_grandchild(data_key);
    if (reduce_level) {
      data_grandchild = StructuredForm(data_value);
      data_grandchild.set_key(data_key);
    } else {
      data_grandchild.SetValue(data_value);
    }

    StructuredForm* data_child = GetChildWithKey(data_section_key);
    if (data_child == nullptr) {
      // first time adding dynamic data to the data section "data_section_key"
      StructuredForm new_data_section_sf(data_section_key);
      AddChild(new_data_section_sf);
      data_child = GetChildWithKey(data_section_key);
      RTC_DCHECK(data_child != nullptr);
    }
    data_child->AddChild(data_grandchild);
  }
};

// DEFINE_ENUMERATED_LOG_OBJECT defines an LogObject-derived type with a
// build-in stringified enum type (see stringifiedenum.h). "enum_name"
// will be the type name and enum_key will be the key for the underlying
// LogObject when it is treated as a key-value pair based StructuredForm.
// Refer to the definition of StructuredForm below.

#define DECLARE_ENUMERATED_LOG_OBJECT(enum_name, enum_key, ...)             \
  DECLARE_STRINGIFIED_ENUM(enum_name##Base, __VA_ARGS__);                   \
  class enum_name final : public enum_name##Base, public LogObject<false> { \
   public:                                                                  \
    enum_name() {}                                                          \
    explicit enum_name(Value enum_val)                                      \
        : enum_name##Base(enum_val), LogObject(#enum_key) {                 \
      std::string enum_val_str = EnumToStr(enum_val);                       \
      if (enum_val == enum_name::kUndefined) {                              \
        value_sf_set_["value"].reset(new StructuredForm("undefined"));      \
        value_sf_set_["comment"].reset(new StructuredForm(enum_val_str));   \
        return;                                                             \
      }                                                                     \
      value_str_ = enum_val_str;                                            \
    }                                                                       \
    enum_name(const enum_name& other)                                       \
        : enum_name##Base(other), LogObject(other) {}                       \
    ~enum_name() final {}                                                   \
  };

#define DEFINE_ENUMERATED_LOG_OBJECT(enum_name, enum_key, ...) \
  DEFINE_STRINGIFIED_ENUM(enum_name##Base, __VA_ARGS__)

// LogEventType
DECLARE_ENUMERATED_LOG_OBJECT(LogEventType,
                              type,
                              kNone,
                              kAny,
                              kCandidateGathered,
                              kConnectionCreated,
                              kConnectionStateChanged,
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
  LogEvent(const LogEvent& other);

  /*virtual*/ LogEvent& operator=(const LogEvent& other);

  // from HasUnboxedInternalData
  /*virtual*/ void BoxInternalDataInConstructor() override;

  void AddHookForDownstreamEvents(const LogHook& hook);
  void UpdateUpstreamEvents();
  // The event-hook attachment is based on the matching of signature that
  // consists of a set of key-value pairs.
  // The signature value can be either a string or a StructuredForm
  template <typename signature_value_type>
  void AddSignatureForUpstreamHook(
      const std::string& signature_key,
      const signature_value_type& signature_value) {
    AddData(signature_key, signature_value, false, "signature");
  }

  std::string id() const { return id_; }
  LogEventType::Value type() const { return type_.value(); }
  Timestamp created_at() const { return event_created_at_; }
  std::unordered_set<LogEvent*> upstream_events() const {
    return upstream_events_;
  }

  ~LogEvent() final;

  // internal customized data
  ADD_UNBOXED_DATA_WITHOUT_SETTER(std::string, id);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(Timestamp, event_created_at);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(LogEventType, type);
  ADD_UNBOXED_DATA_AND_DECLARE_SETTER(std::unordered_set<LogEvent*>,
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
  LogHook(const LogHook& other);
  explicit LogHook(LogEvent* originating_event,
                   LogEventType::Value downstream_event_type);

  // from HasUnboxedInternalData
  /*virtual*/ void BoxInternalDataInConstructor() override;

  // The signature can be matched to an event signature added by
  // LogEvent::AddSignatureForUpstreamHook for attachment
  template <typename signature_value_type>
  void AddSignatureForDownstreamEvent(
      const std::string& signature_key,
      const signature_value_type& signature_value) {
    AddData(signature_key, signature_value, false, "signature");
  }
  bool CanBeAttachedByDownstreamEvent(const LogEvent& event) const;

  Timestamp valid_from() const { return hook_valid_from_; }
  LogEvent* originating_event() const { return originating_event_; }
  LogEventType::Value downstream_event_type() const {
    return downstream_event_type_;
  }

  ~LogHook() final;

  // internal customized data
  ADD_UNBOXED_DATA_WITHOUT_SETTER(Timestamp, hook_valid_from);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(LogEvent*, originating_event);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(LogEventType::Value, downstream_event_type);

 private:
  void Init(const LogHook& other);

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

// base class for identifiers in logging
class LogIdentifier : public LogObject<false>,
                      public Comparable<LogIdentifier> {
 public:
  LogIdentifier();
  explicit LogIdentifier(const std::string& id);
  LogIdentifier(const LogIdentifier& other);

  CompareResult Compare(const LogIdentifier& other) const override;

  std::string id() const { return id_; }

  ~LogIdentifier() override;

  ADD_UNBOXED_DATA_AND_DECLARE_SETTER(std::string, id);
};

}  // namespace icelog

}  // namespace webrtc

#endif  // LOGGING_ICE_LOG_ICELOGTYPE_H_
