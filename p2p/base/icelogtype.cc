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

const StructuredForm kNullStructuredForm("null");

StructuredForm StructuredForm::SetValueAsString(const std::string& value_str) {
  StructuredForm original = *this;
  value_ = value_str;
  return original;
}

StructuredForm StructuredForm::SetValueAsStructuredForm(
    const StructuredForm& child) {
  StructuredForm original = *this;
  // reset the value
  value_ = Json::Value::null;
  value_[child.key_] = child.value_;
  return original;
}

StructuredForm StructuredForm::InsertStructuredFormToValue(
    const StructuredForm& child) {
  StructuredForm rtrn = *this;
  if (isStump()) {
    value_ = Json::Value::null;
  } else if (!value_[child.key_].isNull()) {
    // jsoncpp implementation specific
    rtrn = kNullStructuredForm;
  }
  value_[child.key_] = child.value_;
  return rtrn;
}

StructuredForm StructuredForm::GetStructuredFormFromValue(
    const std::string& key_child) const {
  if (isStump()) {
    return kNullStructuredForm;
  }
  // jsoncpp implementation specific: the const version of operator[] is called
  // since the outer const qualification. Null is returned if the key does not
  // exist
  if (value_[key_child] == Json::Value::null) {
    return kNullStructuredForm;
  }
  return StructuredForm(key_child, value_[key_child]);
}

Json::Value StructuredForm::as_json() const {
  Json::Value as_json;
  as_json[key_] = value_;
  return as_json;
}

bool StructuredForm::isStump() const {
  SanityCheckWhenUsingJsonImplementation();
  return !(value_.isNull()) && value_.isString();
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

void StructuredForm::SanityCheckWhenUsingJsonImplementation() const {
  RTC_DCHECK(value_.isNull() || value_.isString() || value_.isObject());
}

// LogObject
// template<typename has_unboxed_internal_data>
// LogObject::LogObject() {}
// template<typename has_unboxed_internal_data>
// LogObject::LogObject(const std::string key) : StructuredForm(key) {}
// template<typename has_unboxed_internal_data>
// LogObject::LogObject(const LogObject& other) : StructuredForm(other) {}

// LogEvent
LogEvent::LogEvent(const LogEventType& type)
    : LogObject("event"),
      event_created_at_(rtc::SystemTimeNanos()),
      type_(new LogEventType(type)) {
  id_ = CreateRandomAlphaString(3) + std::to_string(event_created_at_);
  BoxInternalData();
}

LogEvent::LogEvent(const LogEvent& other) {
  operator=(other);
}

LogEvent& LogEvent::operator=(const LogEvent& other) {
  LogObject::operator=(other);
  id_ = other.id_;
  event_created_at_ = other.event_created_at_;
  type_.reset(new LogEventType(*(other.type_)));
  upstream_events_ = other.upstream_events_;
  return *this;
}

void LogEvent::BoxInternalData() {
  // box the event-specific data into the underlying structured form
  StructuredForm id_sf("id");
  id_sf.SetValueAsString(id_);
  StructuredForm created_at_sf("created_at");
  created_at_sf.SetValueAsString(std::to_string(event_created_at_));
  StructuredForm upstream_events_sf("upstream_events");
  upstream_events_sf.SetValueAsString("");
  StructuredForm data_sf("data");
  data_sf.SetValueAsString("");
  InsertStructuredFormToValue(id_sf);
  InsertStructuredFormToValue(created_at_sf);
  InsertStructuredFormToValue(*type_);
  InsertStructuredFormToValue(upstream_events_sf);
  InsertStructuredFormToValue(data_sf);
}

void LogEvent::AddHookForDownstreamEvents(const LogHook& hook) {
  LogHookPool::Instance()->RegisterEventHook(hook);
}

void LogEvent::UpdateUpstreamEvents() {
  upstream_events_ =
      LogHookPool::Instance()->GetUpstreamEventsForAnEvent(*this);
  StructuredForm upstream_events_sf("upstream_events");
  std::string upstream_events_str;
  for (auto ue : upstream_events_) {
    upstream_events_str += ue->id_ + ",";
  }
  upstream_events_str =
      upstream_events_str.substr(0, upstream_events_str.size() - 1);
  upstream_events_sf.SetValueAsString(upstream_events_str);
  InsertStructuredFormToValue(upstream_events_sf);
}

void LogEvent::AddData(const std::string key,
                       const StructuredForm& data_as_value,
                       bool reduce_level) {
  StructuredForm consolidated_data(key);
  if (reduce_level) {
    consolidated_data = data_as_value;
    consolidated_data.set_key(key);
  } else {
    consolidated_data.SetValueAsStructuredForm(data_as_value);
  }
  // todo(qingsi) GetStructuredFormFromValue just returns a copy since there is
  // no real chidl structured form in the parent in implementation. This is
  // really pain in the butt and the workaround like below is tedious and error
  // prone
  StructuredForm current_data = GetStructuredFormFromValue("data");
  current_data.InsertStructuredFormToValue(consolidated_data);
  InsertStructuredFormToValue(current_data);
}

LogEvent::~LogEvent() {}

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
  BoxInternalData();
}

void LogHook::BoxInternalData() {
  // box the hook-specific data into the underlying structured form
  StructuredForm valid_from_sf("valid_from");
  valid_from_sf.SetValueAsString(std::to_string(hook_valid_from_));
  StructuredForm originating_event_sf("originating_event_id");
  originating_event_sf.SetValueAsString(originating_event_->id());
  StructuredForm downstream_event_tf("downstream_event_type");
  downstream_event_tf.SetValueAsString(
      LogEventType::EnumToStr(downstream_event_type_));
  StructuredForm constraint_sf("constraint");
  constraint_sf.SetValueAsString("");
  InsertStructuredFormToValue(valid_from_sf);
  InsertStructuredFormToValue(originating_event_sf);
  InsertStructuredFormToValue(downstream_event_tf);
  InsertStructuredFormToValue(constraint_sf);
}

void LogHook::SetDownstreamEventType(LogEventType::Value type) {
  downstream_event_type_ = type;
}

void LogHook::AddConstraintForDownstreamEvent(
    const StructuredForm& constraint) {
  StructuredForm current_constraint_set =
      GetStructuredFormFromValue("constraint");
  current_constraint_set.InsertStructuredFormToValue(constraint);
  InsertStructuredFormToValue(current_constraint_set);
}

bool LogHook::CanBeAttachedByADownstreamEvent(const LogEvent& event) const {
  return event.type() == downstream_event_type_ &&
         event.created_at() >= hook_valid_from_;
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
    if (hook.CanBeAttachedByADownstreamEvent(event)) {
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
  InsertStructuredFormToValue(id_->ToStructuredForm());
  InsertStructuredFormToValue(type_->ToStructuredForm());
  InsertStructuredFormToValue(network_->ToStructuredForm());
  InsertStructuredFormToValue(protocol_->ToStructuredForm());
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
  InsertStructuredFormToValue(id_->ToStructuredForm());
  StructuredForm local_cand_sf("local_candidate");
  local_cand_sf.SetValueAsStructuredForm(
      local_cand_property_->ToStructuredForm());
  InsertStructuredFormToValue(local_cand_sf);
  StructuredForm remote_cand_sf("remote_candidate");
  remote_cand_sf.SetValueAsStructuredForm(
      remote_cand_property_->ToStructuredForm());
  InsertStructuredFormToValue(remote_cand_sf);
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
  InsertStructuredFormToValue(desc_sf);
  InsertStructuredFormToValue(data_sf);
}

LogMessage::LogMessage() : LogObject("message") {
  Init();
}

LogMessage& LogMessage::SetDescription(const std::string& desc) {
  StructuredForm desc_sf("description");
  desc_sf.SetValueAsString(desc);
  InsertStructuredFormToValue(desc_sf);
  return *this;
}

// set a plain message with string as it data
LogMessage& LogMessage::SetData(const std::string& data_str) {
  SetDescription("plain");
  StructuredForm data_sf("data");
  data_sf.SetValueAsString(data_str);
  InsertStructuredFormToValue(data_sf);
  return *this;
}

// set a message with data in structured form
LogMessage& LogMessage::SetData(const StructuredForm& data) {
  StructuredForm data_sf("data");
  data_sf.SetValueAsStructuredForm(data);
  InsertStructuredFormToValue(data_sf);
  return *this;
}

// set a message with data containing a set of structured form
LogMessage& LogMessage::SetData(const std::vector<StructuredForm>& data_list) {
  StructuredForm data_sf("data");
  for (auto d : data_list) {
    data_sf.InsertStructuredFormToValue(d);
  }
  InsertStructuredFormToValue(data_sf);
  return *this;
}

}  // namespace icelog

}  // namespace webrtc
