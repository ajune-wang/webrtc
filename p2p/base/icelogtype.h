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

#include <memory>
#include <string>

#include "p2p/base/candidate.h"
#include "p2p/base/port.h"
#include "p2p/base/stringifiedenum.h"
#include "rtc_base/json.h"

namespace webrtc {

namespace icelog {

// DEFINE_ENUMERATED_ICE_OBJECT defines an IceObject-derived type with a
// build-in stringified enum type. "enum_name" will be the type name and
// enum_key will be the key for the underlying IceObject when it is
// treated as a key-value pair based StructuredForm. Refer to the
// definition of StructuredForm below.

#define DEFINE_ENUMERATED_ICE_OBJECT(enum_name, enum_key, ...)       \
  DEFINE_STRINGIFIED_ENUM(enum_name##Base, __VA_ARGS__);             \
  class enum_name : public enum_name##Base, public IceObject {       \
   public:                                                           \
    explicit enum_name(INTERNAL_ENUM_NAME(enum_name##Base) enum_val) \
        : enum_name##Base(enum_val), IceObject(#enum_key) {          \
      std::string enum_val_str = EnumToStr(enum_val);                \
      if (enum_val == enum_name::kUndefined) {                       \
        IceObject::value_["value"] = "undefined";                    \
        IceObject::value_["comment"] = "[" + enum_val_str + "]";     \
        return;                                                      \
      }                                                              \
      IceObject::value_ = enum_val_str;                              \
    }                                                                \
    enum_name(const enum_name& other)                                \
        : enum_name##Base(other), IceObject(other) {}                \
    ~enum_name() {}                                                  \
  };

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
//  2) the value is a literal string or a collection of StructuredForm's
// The current implementation utilizes JSON object as the infrasture
class StructuredForm : public Describable {
 public:
  StructuredForm() : key_(""), value_(Json::Value::null) {}
  StructuredForm(const std::string key,
                 const std::string value_str = std::string(),
                 bool is_value_literal = true);
  StructuredForm(const StructuredForm& other) { operator=(other); }

  StructuredForm& operator=(const StructuredForm& other) {
    key_ = other.key_;
    value_ = other.value_;
    return *this;
  }

  // This method is to implement the conceptual structure described above
  // when the value of a StructuredForm is a collection of StructuredForm's
  // and this method adds a StructuredForm to this collection
  StructuredForm& AppendToValue(const StructuredForm& child);

  Json::Value as_json() const;
  std::string key() const { return key_; }

  /*virtual*/ std::string ToString() const override;
  /*virtual*/ StructuredForm ToStructuredForm() const override { return *this; }

 protected:
  std::string key_;
  Json::Value value_;
};

class IceObject : public StructuredForm {
 public:
  IceObject();
  explicit IceObject(const std::string key);
  IceObject(const IceObject& other);

  virtual IceObject& operator=(const IceObject& other) {
    StructuredForm::operator=(other);
    return *this;
  }

  virtual ~IceObject() {}
};

// Reason to explain the result of a comparison between Comparable's
class Reason final : public IceObject {
 public:
  Reason();
  void set_description(const std::string& desc);
};

using ReasonPtr = std::unique_ptr<Reason>;

enum class CompareResult { kLess, kGreater, kEqual };

template <typename T>
class Comparable {
 public:
  virtual CompareResult Compare(const T& other,
                                ReasonPtr reason_for_result) const = 0;
  virtual bool operator<(const T& other) const {
    return Compare(other, nullptr) == CompareResult::kLess;
  }
  virtual bool operator>(const T& other) const {
    return Compare(other, nullptr) == CompareResult::kGreater;
  }
  virtual bool operator==(const T& other) const {
    return Compare(other, nullptr) == CompareResult::kEqual;
  }
  virtual bool operator!=(const T& other) const {
    return Compare(other, nullptr) != CompareResult::kEqual;
  }
  virtual ~Comparable() {}
};

// base class for identifiers
class IceIdentifier : public IceObject, public Comparable<IceIdentifier> {
 public:
  IceIdentifier();
  explicit IceIdentifier(const std::string& id);
  IceIdentifier(const IceIdentifier& other);

  CompareResult Compare(const IceIdentifier& other,
                        ReasonPtr = nullptr) const override;

  virtual std::string id() const { return id_; }

  virtual ~IceIdentifier() {}

 protected:
  // using id_ to avoid excessive boxing and unboxing when fetching
  // the id string
  std::string id_;
  // set the plain string id and also box it in value_
  void set_id(const std::string& id);
};

// dedicated identifier type for connections
class IceCandidateId final : public IceIdentifier,
                             public Comparable<IceCandidateId> {
 public:
  IceCandidateId();
  explicit IceCandidateId(const std::string& id);

  using IceIdentifier::Compare;
  CompareResult Compare(const IceCandidateId& other, ReasonPtr) const override;
};

// dedicated identifier type for connections
class IceConnectionId final : public IceIdentifier,
                              public Comparable<IceConnectionId> {
 public:
  IceConnectionId();
  explicit IceConnectionId(const std::string& id);
  explicit IceConnectionId(cricket::Connection* conn);
  IceConnectionId(const IceCandidateId& local_cand_id,
                  const IceCandidateId& remote_cand_id);

  using IceIdentifier::Compare;
  CompareResult Compare(const IceConnectionId& other, ReasonPtr) const override;
};

// the following declared stringified enum classes are defined in .cc using
// the helper macros from stringifiedenum.h
class IceCandidateContent;
class IceCandidateProtocol;
class IceCandidateType;
class IceCandidateNetwork;

class IceCandidateProperty final : public IceObject,
                                   public Comparable<IceCandidateProperty> {
 public:
  IceCandidateProperty();
  IceCandidateProperty(const cricket::Port& port, const cricket::Candidate& c);

  CompareResult Compare(const IceCandidateProperty& other,
                        ReasonPtr reason) const override;

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

class IceConnectionProperty final : public IceObject,
                                    public Comparable<IceConnectionProperty> {
 public:
  IceConnectionProperty();
  IceConnectionProperty(const IceCandidateProperty& local_cand_property,
                        const IceCandidateProperty& remote_cand_property);

  CompareResult Compare(const IceConnectionProperty& other,
                        ReasonPtr reason) const override;

 private:
  std::unique_ptr<IceConnectionId> id_;
  const IceCandidateProperty* local_cand_property_;
  const IceCandidateProperty* remote_cand_property_;
  StructuredForm as_json_;
  // bool is_selected;
};

}  // namespace icelog

}  // namespace webrtc

#endif  // P2P_BASE_ICELOGTYPE_H_
