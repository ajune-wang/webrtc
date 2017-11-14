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
#include "rtc_base/basictypes.h"
#include "rtc_base/checks.h"
#include "rtc_base/timeutils.h"

namespace {

// Base 32
const char kAlpha[] = "ABCDEFabcdefghijklmnopqrstuvwxyz";

std::string CreateRandomAlphaString(size_t len) {
  std::string str;
  rtc::CreateRandomString(len, kAlpha, &str);
  return str;
}

}  // unnamed namespace

namespace webrtc {

namespace icelog {

// StructuredForm
StructuredForm::StructuredForm(const std::string key,
                               const std::string value_str,
                               bool is_value_str_literal)
    : key_(key) {
  if (is_value_str_literal) {
    // automatic boxing by value_ as a JSON::Value
    value_ = value_str.empty() ? Json::Value::null : value_str;
    return;
  }
  Json::Reader reader;
  if (!reader.parse(value_str, value_, false)) {
    value_ = Json::Value::null;
  }
}

StructuredForm::StructuredForm(const std::string& key, Json::Value value) {
  key_ = key;
  value_ = value;
  for (const std::string& child_key : value.getMemberNames()) {
    child_keys_.insert(child_key);
  }
}

void StructuredForm::Init(const StructuredForm& other) {
  key_ = other.key_;
  value_ = other.value_;
  child_keys_ = other.child_keys_;
}

StructuredForm StructuredForm::SetValueAsString(const std::string& value_str) {
  StructuredForm original = *this;
  value_ = value_str;
  return original;
}

StructuredForm StructuredForm::SetValueAsStructuredForm(
    const StructuredForm& child) {
  StructuredForm original = *this;
  // jsoncpp implementation specific: reset the value
  value_ = Json::Value::null;
  value_[child.key_] = child.value_;
  return original;
}

bool StructuredForm::HasChildWithKey(const std::string child_key) const {
  return !IsStump()  // this line may not be necessary
                     /* jsoncpp implementation specific */
         && value_.isMember(child_key);
}

bool StructuredForm::AddChild(const StructuredForm& child) {
  if (IsStump()) {
    return false;
  }
  value_[child.key_] = child.value_;
  child_keys_.insert(child.key_);
  std::cout << "add " << child.key_ << " to " << key_ << std::endl;
  return true;
}

bool StructuredForm::UpdateChild(const StructuredForm& child) {
  if (!HasChildWithKey(child.key_)) {
    return false;
  }
  RTC_DCHECK(AddChild(child));
  return true;
}

// todo(qingsi) the copy used here is bug prone!
StructuredForm StructuredForm::GetChildWithKey(
    const std::string& child_key) const {
  if (!HasChildWithKey(child_key)) {
    return kNullStructuredForm;
  }
  return StructuredForm(child_key, value_[child_key]);
}

bool StructuredForm::IsStump() const {
  SanityCheckWhenUsingJsonImplementation();
  return !(value_.isNull()) && value_.isString();
}

bool StructuredForm::IsNull() const {
  return operator==(kNullStructuredForm);
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

Json::Value StructuredForm::as_json() const {
  Json::Value as_json;
  as_json[key_] = value_;
  return as_json;
}

void StructuredForm::SanityCheckWhenUsingJsonImplementation() const {
  RTC_DCHECK(value_.isNull() || value_.isString() || value_.isObject());
}

// LogEvent
LogEvent::LogEvent(const LogEventType& type)
    : LogObject("event"),
      event_created_at_(rtc::SystemTimeNanos()),
      type_(type) {
  id_ = CreateRandomAlphaString(3) + std::to_string(event_created_at_);
  BoxInternalDataInConstructor();
}

LogEvent::LogEvent(const LogEvent& other) : LogObject(other) {
  Init(other);
}

void LogEvent::Init(const LogEvent& other) {
  id_ = other.id_;
  event_created_at_ = other.event_created_at_;
  type_ = other.type_;
  upstream_events_ = other.upstream_events_;
}

LogEvent& LogEvent::operator=(const LogEvent& other) {
  LogObject::operator=(other);
  Init(other);
  return *this;
}

void LogEvent::BoxInternalDataInConstructor() {
  // box the event-specific data into the underlying structured form
  StructuredForm id_sf("id");
  id_sf.SetValueAsString(id_);
  StructuredForm created_at_sf("created_at");
  created_at_sf.SetValueAsString(std::to_string(event_created_at_));
  StructuredForm upstream_events_sf("upstream_events");
  upstream_events_sf.SetValueAsString("");
  AddChild(id_sf);
  AddChild(created_at_sf);
  AddChild(type_);  //  type_ is a StructuredForm
  AddChild(upstream_events_sf);
}

void LogEvent::AddHookForDownstreamEvents(const LogHook& hook) {
  LogHookPool::Instance()->RegisterEventHook(hook);
}

void LogEvent::AddSignature(const std::string& signature_key,
                            const StructuredForm& signature_value) {
  StructuredForm signature_grandchild(signature_key);
  signature_grandchild.SetValueAsStructuredForm(signature_value);

  StructuredForm signature_child = GetChildWithKey("signature");
  if (signature_child.IsNull()) {
    // first time adding signature
    signature_child = StructuredForm("signature");
    RTC_DCHECK(AddChild(signature_child));
  }
  RTC_DCHECK(signature_child.AddChild(signature_grandchild));
  RTC_DCHECK(UpdateChild(signature_child));
}

// from ADD_UNBOXED_DATA_WITH_UNDEFINED_SETTER
void LogEvent::set_upstream_events(
    const std::unordered_set<LogEvent*>& upstream_events) {
  upstream_events_ = upstream_events;
  StructuredForm upstream_events_sf("upstream_events");
  std::string upstream_events_str;
  for (auto ue : upstream_events_) {
    upstream_events_str += ue->id_ + ",";
  }
  // remove the trailing comma
  upstream_events_str =
      upstream_events_str.substr(0, upstream_events_str.size() - 1);
  upstream_events_sf.SetValueAsString(upstream_events_str);
  AddChild(upstream_events_sf);
}

void LogEvent::UpdateUpstreamEvents() {
  set_upstream_events(
      LogHookPool::Instance()->GetUpstreamEventsForAnEvent(*this));
}

// LogEventPool
LogEventPool::LogEventPool() {}

LogEventPool* LogEventPool::Instance() {
  RTC_DEFINE_STATIC_LOCAL(LogEventPool, instance, ());
  return &instance;
}

LogEventPool::~LogEventPool() {
  // By above RTC_DEFINE_STATIC_LOCAL.
  RTC_NOTREACHED() << "LogEventPool should never be destructed.";
}

LogEvent* LogEventPool::RegisterEvent(const LogEvent& event) {
  internal_event_pool_.push_back(event);
  return &(internal_event_pool_.back());
}

// LogHook
DEFINE_ENUMERATED_ICE_OBJECT(LogEventType,
                             type,
                             kNone,
                             kAny,
                             kCandidateGathered,
                             kConnectionCreated,
                             kStunBindRequestSent,
                             kStunBindRequestResponseReceived,
                             kConnectionReselected,
                             kNumLogEventTypes);

LogHook::LogHook(LogEvent* originating_event,
                 LogEventType::Value downstream_event_type)
    : LogObject("hook"),
      hook_valid_from_(originating_event->created_at()),
      originating_event_(originating_event),
      downstream_event_type_(downstream_event_type) {
  BoxInternalDataInConstructor();
}

void LogHook::BoxInternalDataInConstructor() {
  // box the hook-specific data into the underlying structured form
  StructuredForm valid_from_sf("valid_from");
  valid_from_sf.SetValueAsString(std::to_string(hook_valid_from_));
  StructuredForm originating_event_sf("originating_event_id");
  originating_event_sf.SetValueAsString(originating_event_->id());
  StructuredForm downstream_event_tf("downstream_event_type");
  downstream_event_tf.SetValueAsString(
      LogEventType::EnumToStr(downstream_event_type_));
  AddChild(valid_from_sf);
  AddChild(originating_event_sf);
  AddChild(downstream_event_tf);
}

void LogHook::AddConstraintForDownstreamEvent(
    const std::string& constraint_key,
    const StructuredForm& constraint_value) {
  AddData(constraint_key, constraint_value);
}

bool LogHook::CanBeAttachedByDownstreamEvent(const LogEvent& event) const {
  if (event.type() != downstream_event_type_ ||
      event.created_at() < hook_valid_from_) {
    return false;
  }
  // the constraints are stored as data in a hook
  StructuredForm constraint = GetChildWithKey("data");
  StructuredForm event_signature = event.GetChildWithKey("signature");
  for (std::string child_key : constraint.child_keys()) {
    RTC_DCHECK(constraint.HasChildWithKey(child_key));
    if (!event_signature.HasChildWithKey(child_key) ||
        event_signature.GetChildWithKey(child_key) !=
            constraint.GetChildWithKey(child_key)) {
      return false;
    }
  }
  return true;
}

size_t LogHook::HashCode() const {
  return std::hash<std::string>()(
      std::to_string(this->valid_from()) + ":" +
      std::to_string(this->originating_event()->created_at()) + ":" +
      CreateRandomAlphaString(3));
}

size_t LogHook::Hasher::operator()(const LogHook& hook) const {
  return hook.HashCode();
}

bool LogHook::Comparator::operator()(const LogHook& lhs,
                                     const LogHook& rhs) const {
  return lhs.HashCode() == rhs.HashCode();
}

// LogHookPool
LogHookPool::LogHookPool() {}

LogHookPool* LogHookPool::Instance() {
  RTC_DEFINE_STATIC_LOCAL(LogHookPool, instance, ());
  return &instance;
}

LogHookPool::~LogHookPool() {
  // By above RTC_DEFINE_STATIC_LOCAL.
  RTC_NOTREACHED() << "LogHookPool should never be destructed.";
}

void LogHookPool::RegisterEventHook(const LogHook& hook) {
  internal_hook_pool_.insert(hook);
}

std::unordered_set<LogEvent*> LogHookPool::GetUpstreamEventsForAnEvent(
    const LogEvent& event) const {
  std::unordered_set<LogEvent*> ret;
  for (auto& hook : internal_hook_pool_) {
    if (hook.CanBeAttachedByDownstreamEvent(event)) {
      ret.insert(hook.originating_event());
    }
  }
  return ret;
}

// LogIdentifier
LogIdentifier::LogIdentifier() : LogObject("id") {}
LogIdentifier::LogIdentifier(const LogIdentifier& other)
    : LogObject(other), id_(other.id()) {}
LogIdentifier::LogIdentifier(const std::string& id) : LogObject("id") {
  // Note that the id string may contain characters that should be escaped
  // for parsing in postprocessing, depending on the implementation of the
  // structured form
  set_id(id);
}

void LogIdentifier::set_id(const std::string& id) {
  id_ = id;
  // value_ boxes the plain string id
  value_ = id_;
}

CompareResult LogIdentifier::Compare(const LogIdentifier& other) const {
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
IceCandidateId::IceCandidateId(const std::string& id) : LogIdentifier(id) {}

CompareResult IceCandidateId::Compare(const IceCandidateId& other) const {
  return LogIdentifier::Compare(static_cast<const LogIdentifier&>(other));
}

// IceConnectionId
IceConnectionId::IceConnectionId() {}
IceConnectionId::IceConnectionId(const std::string& id) : LogIdentifier(id) {}
IceConnectionId::IceConnectionId(cricket::Connection* conn) {
  set_id(conn->local_candidate().id() + conn->remote_candidate().id());
}
IceConnectionId::IceConnectionId(const IceCandidateId& local_cand_id,
                                 const IceCandidateId& remote_cand_id) {
  set_id(local_cand_id.id() + remote_cand_id.id());
}

CompareResult IceConnectionId::Compare(const IceConnectionId& other) const {
  return LogIdentifier::Compare(static_cast<const LogIdentifier&>(other));
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
    : LogObject("candidate"),
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
  AddChild(id_->ToStructuredForm());
  AddChild(type_->ToStructuredForm());
  AddChild(network_->ToStructuredForm());
  AddChild(protocol_->ToStructuredForm());
}

// todo(qingsi) this is a placeholder
// a full implementation should refer to SortConnectionsAndUpdateState in
// p2ptransportchannel.h
CompareResult IceCandidateProperty::Compare(
    const IceCandidateProperty& other) const {
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
    : LogObject("connection"),
      id_(new IceConnectionId(local_cand_property.id(),
                              remote_cand_property.id())),
      local_cand_property_(&local_cand_property),
      remote_cand_property_(&remote_cand_property) {
  AddChild(id_->ToStructuredForm());
  StructuredForm local_cand_sf("local_candidate");
  local_cand_sf.SetValueAsStructuredForm(
      local_cand_property_->ToStructuredForm());
  AddChild(local_cand_sf);
  StructuredForm remote_cand_sf("remote_candidate");
  remote_cand_sf.SetValueAsStructuredForm(
      remote_cand_property_->ToStructuredForm());
  AddChild(remote_cand_sf);
}

// todo(qingsi) this is a placeholder
// a full implementation should refer to SortConnectionsAndUpdateState in
// p2ptransportchannel.h
CompareResult IceConnectionProperty::Compare(
    const IceConnectionProperty& other) const {
  return CompareResult::kLess;
}

// LogMessage

void LogMessage::Init() {
  StructuredForm desc_sf("description");
  StructuredForm data_sf("data");
  AddChild(desc_sf);
  AddChild(data_sf);
}

LogMessage::LogMessage() : LogObject("message") {
  Init();
}

LogMessage& LogMessage::SetDescription(const std::string& desc) {
  StructuredForm desc_sf("description");
  desc_sf.SetValueAsString(desc);
  AddChild(desc_sf);
  return *this;
}

// set a plain message with string as it data
LogMessage& LogMessage::SetData(const std::string& data_str) {
  SetDescription("plain");
  StructuredForm data_sf("data");
  data_sf.SetValueAsString(data_str);
  AddChild(data_sf);
  return *this;
}

// set a message with data in structured form
LogMessage& LogMessage::SetData(const StructuredForm& data) {
  StructuredForm data_sf("data");
  data_sf.SetValueAsStructuredForm(data);
  AddChild(data_sf);
  return *this;
}

// set a message with data containing a set of structured form
LogMessage& LogMessage::SetData(const std::vector<StructuredForm>& data_list) {
  StructuredForm data_sf("data");
  for (auto d : data_list) {
    data_sf.AddChild(d);
  }
  AddChild(data_sf);
  return *this;
}

}  // namespace icelog

}  // namespace webrtc
