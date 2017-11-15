/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sstream>

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
StructuredForm::StructuredForm()
    : key_(std::string()), value_str_(std::string()), is_stump_(true) {
  value_sf_.clear();
  child_keys_.clear();
}

StructuredForm::StructuredForm(const std::string key)
    : key_(key), value_str_(std::string()), is_stump_(true) {
  value_sf_.clear();
  child_keys_.clear();
}

void StructuredForm::Init(const StructuredForm& other) {
  // deep copy
  key_ = other.key_;
  value_str_ = other.value_str_;
  value_sf_.clear();
  if (!other.value_sf_.empty()) {
    for (auto it = other.value_sf_.begin(); it != other.value_sf_.end(); ++it) {
      value_sf_[it->first].reset(new StructuredForm(*(it->second)));
    }
  }
  child_keys_ = other.child_keys_;
  is_stump_ = other.is_stump_;
}

bool StructuredForm::operator==(const StructuredForm& other) const {
  if (key_ != other.key_ || value_str_ != other.value_str_ ||
      child_keys_ != other.child_keys_) {
    return false;
  }

  for (const std::string& child_key : child_keys_) {
    RTC_DCHECK(value_sf_.find(child_key) != value_sf_.end());
    if (other.value_sf_.find(child_key) == other.value_sf_.end()) {
      return false;
    }
    if (*value_sf_.at(child_key) != *other.value_sf_.at(child_key)) {
      return false;
    }
  }
  return true;
}

StructuredForm StructuredForm::SetValueAsString(const std::string& value_str) {
  StructuredForm original = *this;
  value_str_ = value_str;
  value_sf_.clear();
  is_stump_ = true;
  return original;
}

StructuredForm StructuredForm::SetValueAsStructuredForm(
    const StructuredForm& child) {
  StructuredForm original = *this;
  value_str_ = std::string();
  value_sf_.clear();
  value_sf_[child.key_].reset(new StructuredForm(child));
  child_keys_.insert(child.key_);
  is_stump_ = false;
  return original;
}

bool StructuredForm::HasChildWithKey(const std::string child_key) const {
  bool ret = child_keys_.find(child_key) != child_keys_.end();
  RTC_DCHECK(ret == (value_sf_.find(child_key) != value_sf_.end()));
  return ret;
}

void StructuredForm::AddChild(const StructuredForm& child) {
  value_str_ = std::string();
  is_stump_ = false;
  value_sf_[child.key_].reset(new StructuredForm(child));
  child_keys_.insert(child.key_);
}

bool StructuredForm::UpdateChild(const StructuredForm& child) {
  if (!HasChildWithKey(child.key_)) {
    return false;
  }
  AddChild(child);
  return true;
}

StructuredForm* StructuredForm::GetChildWithKey(
    const std::string& child_key) const {
  if (!HasChildWithKey(child_key)) {
    return nullptr;
  }
  return value_sf_.at(child_key).get();
}

bool StructuredForm::IsStump() const {
  return is_stump_;
}

bool StructuredForm::IsNull() const {
  return operator==(kNullStructuredForm);
}

std::string StructuredForm::ToString() const {
  std::stringstream ss;
  std::string ret;
  ss << "{"
     << "\"" << key_ << "\":";
  if (IsStump()) {
    ss << "\"" << value_str_ << "\"}";
    ret = ss.str();
  } else {
    ss << "{";
    for (const std::string& child_key : child_keys_) {
      RTC_DCHECK(HasChildWithKey(child_key));
      std::string child_sf_str = value_sf_.at(child_key)->ToString();
      // std::cout << child_sf_str << std::endl;
      ss << child_sf_str.substr(1, child_sf_str.size() - 2) << ",";
    }
    ret = ss.str();
    ret.pop_back();
    ret += "}}";
  }
  return ret;
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
  id_sf.SetValue(id_);
  StructuredForm created_at_sf("created_at");
  created_at_sf.SetValue(std::to_string(event_created_at_));
  StructuredForm upstream_events_sf("upstream_events");
  upstream_events_sf.SetValue("");
  AddChild(id_sf);
  AddChild(created_at_sf);
  AddChild(type_);  //  type_ is a StructuredForm
  AddChild(upstream_events_sf);
}

void LogEvent::AddHookForDownstreamEvents(const LogHook& hook) {
  LogHookPool::Instance()->RegisterEventHook(hook);
}

// void LogEvent::AddSignatureForUpstreamHook(
//     const std::string& signature_key,
//     const StructuredForm& signature_value) {
//   AddData(signature_key, signature_value, false, "signature");
// }

// from ADD_UNBOXED_DATA_AND_DECLARE_SETTER
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
  upstream_events_sf.SetValue(upstream_events_str);
  RTC_DCHECK(UpdateChild(upstream_events_sf));
}

void LogEvent::UpdateUpstreamEvents() {
  set_upstream_events(
      LogHookPool::Instance()->GetUpstreamEventsForAnEvent(*this));
}

// LogEventPool
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
DEFINE_ENUMERATED_LOG_OBJECT(LogEventType,
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
  valid_from_sf.SetValue(std::to_string(hook_valid_from_));
  StructuredForm originating_event_sf("originating_event_id");
  originating_event_sf.SetValue(originating_event_->id());
  StructuredForm downstream_event_tf("downstream_event_type");
  downstream_event_tf.SetValue(LogEventType::EnumToStr(downstream_event_type_));
  AddChild(valid_from_sf);
  AddChild(originating_event_sf);
  AddChild(downstream_event_tf);
}

// void LogHook::AddSignatureForDownstreamEvent(
//     const std::string& signature_key,
//     const StructuredForm& signature_value) {
//   AddData(signature_key, signature_value, false, "signature");
// }

bool LogHook::CanBeAttachedByDownstreamEvent(const LogEvent& event) const {
  if (event.type() != downstream_event_type_ ||
      event.created_at() < hook_valid_from_) {
    return false;
  }
  // the hook signature is stored as data in a hook
  StructuredForm* hook_signature = GetChildWithKey("signature");
  StructuredForm* event_signature = event.GetChildWithKey("signature");
  for (std::string child_key : hook_signature->child_keys()) {
    RTC_DCHECK(hook_signature->HasChildWithKey(child_key));
    if (!event_signature->HasChildWithKey(child_key) ||
        *event_signature->GetChildWithKey(child_key) !=
            *hook_signature->GetChildWithKey(child_key)) {
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

// from ADD_UNBOXED_DATA_AND_DECLARE_SETTER
void LogIdentifier::set_id(const std::string& id) {
  id_ = id;
  // boxing
  SetValue(id_);
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

}  // namespace icelog

}  // namespace webrtc
