/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/icelogger.h"

#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"

// currently used as serialization to text logs and postprocessed
#define LOG_ICE(severity) LOG(severity) << IceLogger::ice_log_header

namespace webrtc {

namespace icelog {

const char IceLogger::ice_log_header[] = "[ICE_LOG]: ";

const IceConnectionId kNullIceConnectionId("null");

// IceCandidateId
CompareResult IceCandidateId::Compare(const IceCandidateId& other) const {
  return LogIdentifier::Compare(static_cast<const LogIdentifier&>(other));
}

// IceConnectionId
CompareResult IceConnectionId::Compare(const IceConnectionId& other) const {
  return LogIdentifier::Compare(static_cast<const LogIdentifier&>(other));
}

// enumerated properties
DEFINE_ENUMERATED_LOG_OBJECT(IceCandidateContent,
                             content,
                             kAudio,
                             kVideo,
                             kData);
DEFINE_ENUMERATED_LOG_OBJECT(IceCandidateProtocol,
                             protocol,
                             kUdp,
                             kTcp,
                             kSsltcp,
                             kTls);
DEFINE_ENUMERATED_LOG_OBJECT(IceCandidateType,
                             type,
                             kLocal,
                             kStun,
                             kPrflx,
                             kRelay);
DEFINE_ENUMERATED_LOG_OBJECT(IceCandidateNetwork, network, kWlan, kCell);

// IceCandidateProperty
void IceCandidateProperty::BoxInternalDataInConstructor() {
  StructuredForm is_remote_sf("is_remote");
  is_remote_sf.SetValue(is_remote_ ? "true" : "false");
  AddChild(id_);
  AddChild(type_);
  AddChild(network_);
  AddChild(protocol_);
  AddChild(is_remote_sf);
}

IceCandidateProperty::IceCandidateProperty(const cricket::Port& port,
                                           const cricket::Candidate& c,
                                           bool is_remote)
    : LogObject("candidate"),
      id_(IceCandidateId(c.id())),
      type_(IceCandidateType(IceCandidateType::StrToEnum(c.type()))),
      content_(IceCandidateContent(
          IceCandidateContent::StrToEnum(port.content_name()))),
      protocol_(
          IceCandidateProtocol(IceCandidateProtocol::StrToEnum(c.protocol()))),
      network_(IceCandidateNetwork(
          IceCandidateNetwork::StrToEnum(c.network_name()))),
      is_remote_(is_remote) {
  BoxInternalDataInConstructor();
}

// IceConnectionProperty
DEFINE_ENUMERATED_LOG_OBJECT(IceConnectionState,
                             state,
                             kInactive,
                             kSentCheck,
                             kReceivedCheck,
                             kSentCheckResponse,
                             kReceivedCheckResponse,
                             kSelected);

void IceConnectionProperty::BoxInternalDataInConstructor() {
  LOG(LS_INFO) << "box ice conn property";
  StructuredForm local_cand_sf("local_candidate");
  if (local_candidate_property_ == nullptr) {
    local_cand_sf.SetValue(std::string());
  } else {
    local_cand_sf.SetValue(*local_candidate_property_);
  }
  StructuredForm remote_cand_sf("remote_candidate");
  if (remote_candidate_property_ == nullptr) {
    remote_cand_sf.SetValue(std::string());
  } else {
    remote_cand_sf.SetValue(*remote_candidate_property_);
  }
  AddChild(id_);
  AddChild(local_cand_sf);
  AddChild(remote_cand_sf);
  AddChild(state_);
}

IceConnectionProperty::IceConnectionProperty()
    : local_candidate_property_(nullptr),
      remote_candidate_property_(nullptr),
      state_(IceConnectionState::kInactive) {
  BoxInternalDataInConstructor();
}

IceConnectionProperty::IceConnectionProperty(
    const IceCandidateProperty& local_candidate_property,
    const IceCandidateProperty& remote_candidate_property)
    : LogObject("connection"),
      id_(IceConnectionId(local_candidate_property.id(),
                          remote_candidate_property.id())),
      local_candidate_property_(&local_candidate_property),
      remote_candidate_property_(&remote_candidate_property),
      state_(IceConnectionState::kInactive) {
  BoxInternalDataInConstructor();
}

// IceLogger
IceLogger::IceLogger()
    : hook_pool_(LogHookPool::Instance()),
      event_pool_(LogEventPool::Instance()) {
  connection_property_by_id_[kNullIceConnectionId].reset(
      new IceConnectionProperty());
}

IceLogger* IceLogger::Instance() {
  RTC_DEFINE_STATIC_LOCAL(IceLogger, instance, ());
  return &instance;
}

IceLogger::~IceLogger() {
  // By above RTC_DEFINE_STATIC_LOCAL.
  RTC_NOTREACHED() << "IceLogger should never be destructed.";
}

IceCandidateId IceLogger::RegisterCandidate(cricket::Port* port,
                                            const cricket::Candidate& c,
                                            bool is_remote) {
  IceCandidateId cid(c.id());
  if (candidate_property_by_id_.find(cid) == candidate_property_by_id_.end()) {
    candidate_property_by_id_[cid].reset(
        new IceCandidateProperty(*port, c, is_remote));
  }
  return cid;
}

LogEvent* IceLogger::CreateLogEventAndAddToEventPool(const LogEventType& type) {
  LogEvent event(type);
  return event_pool_->RegisterEvent(event);
}

IceConnectionId IceLogger::RegisterConnection(cricket::Connection* conn) {
  if (conn == nullptr) {
    return kNullIceConnectionId;
  }
  IceConnectionId cnid(conn);
  if (connection_property_by_id_.find(cnid) !=
      connection_property_by_id_.end()) {
    return cnid;
  }
  cricket::Port* local_port = conn->port();
  const cricket::Candidate& local_candidate = conn->local_candidate();
  const IceCandidateId local_candidate_id =
      RegisterCandidate(local_port, local_candidate, false);
  const cricket::Candidate& remote_candidate = conn->remote_candidate();
  const IceCandidateId remote_candidate_id =
      RegisterCandidate(local_port, remote_candidate, true);
  connection_property_by_id_[cnid].reset(new IceConnectionProperty(
      *candidate_property_by_id_[local_candidate_id],
      *candidate_property_by_id_[remote_candidate_id]));
  return cnid;
}

void IceLogger::LogCandidateGathered(cricket::Port* port,
                                     const cricket::Candidate& c) {
  IceCandidateId cid = RegisterCandidate(port, c, false);
  LogEvent* event = CreateLogEventAndAddToEventPool(
      LogEventType(LogEventType::kCandidateGathered));
  event->AddData("candidate", *candidate_property_by_id_[cid], true);
  LogHook hook(event, LogEventType::kConnectionCreated);
  hook.AddSignatureForDownstreamEvent("local_candidate",
                                      *candidate_property_by_id_[cid]);
  event->AddHookForDownstreamEvents(hook);
  event->UpdateUpstreamEvents();
  LOG_ICE(LS_INFO) << hook.ToString();
  LOG_ICE(LS_INFO) << event->ToString();
}

void IceLogger::LogConnectionCreated(cricket::Connection* conn) {
  IceConnectionId cnid = RegisterConnection(conn);
  LogEvent* event = CreateLogEventAndAddToEventPool(
      LogEventType(LogEventType::kConnectionCreated));
  event->AddData("connection", *connection_property_by_id_[cnid], true);
  event->AddSignatureForUpstreamHook(
      "local_candidate",
      *(connection_property_by_id_[cnid]->local_candidate_property()));
  event->UpdateUpstreamEvents();
  LOG_ICE(LS_INFO) << event->ToString();
}

void IceLogger::LogConnectionPingResponseReceived(cricket::Connection* conn) {
  IceConnectionId cnid(conn);
  if (connection_property_by_id_.find(cnid) ==
      connection_property_by_id_.end()) {
    RegisterConnection(conn);
  }
  LogEvent* event = CreateLogEventAndAddToEventPool(
      LogEventType(LogEventType::kStunBindRequestResponseReceived));
  event->AddData("connection", *connection_property_by_id_[cnid], true);
  LogHook hook(event, LogEventType::kConnectionReselected);
  hook.AddSignatureForDownstreamEvent("connection",
                                      *connection_property_by_id_[cnid]);
  event->AddHookForDownstreamEvents(hook);
  event->UpdateUpstreamEvents();
  LOG_ICE(LS_INFO) << event->ToString();
  LOG_ICE(LS_INFO) << hook.ToString();
}

void IceLogger::LogConnectionReselected(cricket::Connection* conn_old,
                                        cricket::Connection* conn_new) {
  IceConnectionId cnid_old = RegisterConnection(conn_old);
  IceConnectionId cnid_new = RegisterConnection(conn_new);

  LogEvent* event = CreateLogEventAndAddToEventPool(
      LogEventType(LogEventType::kConnectionReselected));
  event->AddData("old_connection", *connection_property_by_id_[cnid_old], true);
  event->AddData("new_connection", *connection_property_by_id_[cnid_new], true);
  event->AddSignatureForUpstreamHook("connection",
                                     *connection_property_by_id_[cnid_new]);
  //   event->AddSignatureForUpstreamHook("test", "this is a test for string
  //   valued signature addition.");
  event->UpdateUpstreamEvents();
  LOG_ICE(LS_INFO) << event->ToString();
}

}  // namespace icelog

}  // namespace webrtc
