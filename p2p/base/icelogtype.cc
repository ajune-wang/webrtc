/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/icelogtype.h"

namespace webrtc {

namespace icelog {

// StructuredForm
StructuredForm::StructuredForm(const std::string key,
                               const std::string value_str,
                               bool is_value_literal)
    : key_(key) {
  if (is_value_literal) {
    // automatic boxing by value_
    value_ = value_str.empty() ? Json::Value::null : value_str;
    return;
  }
  Json::Reader reader;
  if (!reader.parse(value_str, value_, false)) {
    value_ = Json::Value::null;
  }
}

StructuredForm& StructuredForm::AppendToValue(const StructuredForm& child) {
  value_[child.key_] = child.value_;
  return *this;
}

Json::Value StructuredForm::as_json() const {
  Json::Value as_json;
  as_json[key_] = value_;
  return as_json;
}

std::string StructuredForm::ToString() const {
  Json::FastWriter writer;
  std::string s = writer.write(as_json());
  // FastWriter writes a newline to the end of the stringified JSON,
  // which can cause parsing failure in deserialization
  if (s.back() == '\n') {
    s = s.substr(0, s.size() - 1);
  }
  return s;
}

// IceObject
IceObject::IceObject() {}
IceObject::IceObject(const std::string key) : StructuredForm(key) {}
IceObject::IceObject(const IceObject& other) : StructuredForm(other) {}

// Reason
Reason::Reason() : IceObject("reason") {}

void Reason::set_description(const std::string& desc) {
  value_ = desc;
}

// IceIdentifier
IceIdentifier::IceIdentifier() : IceObject("id") {}
IceIdentifier::IceIdentifier(const IceIdentifier& other)
    : IceObject(other), id_(other.id()) {}
IceIdentifier::IceIdentifier(const std::string& id) : IceObject("id") {
  // Note that the id string may contain characters that should be escaped
  // for parsing in postprocessing, depending on the implementation of the
  // structured form
  set_id(id);
}

void IceIdentifier::set_id(const std::string& id) {
  id_ = id;
  // value_ boxes the plain string id
  value_ = id_;
}

CompareResult IceIdentifier::Compare(const IceIdentifier& other,
                                     ReasonPtr) const {
  std::string this_id = id();
  std::string other_id = other.id();
  if (this_id < other_id) {
    return CompareResult::kLess;
  } else if (this_id > other_id) {
    return CompareResult::kGreater;
  }
  return CompareResult::kEqual;
}

// IceCandidateId
IceCandidateId::IceCandidateId() {}
IceCandidateId::IceCandidateId(const std::string& id) : IceIdentifier(id) {}

CompareResult IceCandidateId::Compare(const IceCandidateId& other,
                                      ReasonPtr) const {
  return IceIdentifier::Compare(static_cast<const IceIdentifier&>(other));
}

// IceConnectionId
IceConnectionId::IceConnectionId() {}
IceConnectionId::IceConnectionId(const std::string& id) : IceIdentifier(id) {}
IceConnectionId::IceConnectionId(cricket::Connection* conn) {
  set_id(conn->local_candidate().id() + conn->remote_candidate().id());
}
IceConnectionId::IceConnectionId(const IceCandidateId& local_cand_id,
                                 const IceCandidateId& remote_cand_id) {
  set_id(local_cand_id.id() + remote_cand_id.id());
}

CompareResult IceConnectionId::Compare(const IceConnectionId& other,
                                       ReasonPtr) const {
  return IceIdentifier::Compare(static_cast<const IceIdentifier&>(other));
}

// enumerated properties
DEFINE_ENUMERATED_ICE_OBJECT(IceCandidateContent,
                             content,
                             kAudio,
                             kVideo,
                             kData);
DEFINE_ENUMERATED_ICE_OBJECT(IceCandidateProtocol,
                             protocol,
                             kUdp,
                             kTcp,
                             kSsltcp,
                             kTls);
DEFINE_ENUMERATED_ICE_OBJECT(IceCandidateType,
                             type,
                             kLocal,
                             kStun,
                             kPrflx,
                             kRelay);
DEFINE_ENUMERATED_ICE_OBJECT(IceCandidateNetwork, network, kWlan, kCell);

// IceCandidateProperty
IceCandidateProperty::IceCandidateProperty() {}

IceCandidateProperty::IceCandidateProperty(const cricket::Port& port,
                                           const cricket::Candidate& c)
    : IceObject("candidate"),
      id_(new IceCandidateId(c.id())),
      type_(new IceCandidateType(IceCandidateType::StrToEnum(c.type()))),
      content_(new IceCandidateContent(
          IceCandidateContent::StrToEnum(port.content_name()))),
      protocol_(new IceCandidateProtocol(
          IceCandidateProtocol::StrToEnum(c.protocol()))),
      network_(new IceCandidateNetwork(
          IceCandidateNetwork::StrToEnum(c.network_name()))),
      ip_addr_(c.address().ipaddr().ToString()),
      is_remote_(false) {
  AppendToValue(id_->ToStructuredForm());
  AppendToValue(type_->ToStructuredForm());
  AppendToValue(network_->ToStructuredForm());
  AppendToValue(protocol_->ToStructuredForm());
}

// todo(qingsi) this is a placeholder
// a full implementation should refer to SortConnectionsAndUpdateState in
// p2ptransportchannel.h
CompareResult IceCandidateProperty::Compare(const IceCandidateProperty& other,
                                            ReasonPtr reason) const {
  reason.reset(new Reason());
  reason->set_description("comparison in placeholder");
  return CompareResult::kLess;
}

IceCandidateProperty::~IceCandidateProperty() {}

// IceConnectionProperty
IceConnectionProperty::IceConnectionProperty() {
  id_ = nullptr;
}

IceConnectionProperty::IceConnectionProperty(
    const IceCandidateProperty& local_cand_property,
    const IceCandidateProperty& remote_cand_property)
    : IceObject("connection"),
      id_(new IceConnectionId(local_cand_property.id(),
                              remote_cand_property.id())),
      local_cand_property_(&local_cand_property),
      remote_cand_property_(&remote_cand_property) {
  AppendToValue(id_->ToStructuredForm());
  AppendToValue(local_cand_property_->ToStructuredForm());
  AppendToValue(remote_cand_property_->ToStructuredForm());
}

// todo(qingsi) this is a placeholder
// a full implementation should refer to SortConnectionsAndUpdateState in
// p2ptransportchannel.h
CompareResult IceConnectionProperty::Compare(const IceConnectionProperty& other,
                                             ReasonPtr reason) const {
  reason.reset(new Reason());
  reason->set_description("comparison in placeholder");
  return CompareResult::kLess;
}

}  // namespace icelog

}  // namespace webrtc
