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

#define LOG_ICE(severity) LOG(severity) << IceLogger::ice_log_header

namespace webrtc {

namespace icelog {

const char IceLogger::ice_log_header[] = "[ICE_LOG]: ";

const IceConnectionId kNullIceConnectionId;

// IceLogger
IceLogger::IceLogger()
    : hook_pool_(LogHookPool::Instance()),
      event_pool_(LogEventPool::Instance()) {
  connection_property_by_id_[kNullIceConnectionId].reset(
      new IceConnectionProperty());
}

IceCandidateId IceLogger::RegisterCandidate(cricket::Port* port,
                                            const cricket::Candidate& c,
                                            bool is_remote) {
  IceCandidateId cid(c.id());
  if (candidate_property_by_id_.find(cid) == candidate_property_by_id_.end()) {
    candidate_property_by_id_[cid].reset(new IceCandidateProperty(*port, c));
    candidate_property_by_id_[cid]->set_is_remote(is_remote);
  }
  return cid;
}

LogEvent* IceLogger::CreateLogEventAndAddToEventPool(const LogEventType& type) {
  LogEvent event(type);
  event.UpdateUpstreamEvents();
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
  // LOG_ICE(LS_INFO) << LogMessage()
  //                         .SetDescription("connection registration")
  //                         .SetData(*connection_property_by_id_[cnid])
  //                         .ToString();
  return cnid;
}

void IceLogger::LogCandidateGathered(cricket::Port* port,
                                     const cricket::Candidate& c) {
  IceCandidateId cid = RegisterCandidate(port, c, false);
  LogEvent* event = CreateLogEventAndAddToEventPool(
      LogEventType(LogEventType::kCandidateGathered));
  event->AddData("candidate", *candidate_property_by_id_[cid], true);
  LogHook hook(event, LogEventType::kConnectionCreated);
  event->AddHookForDownstreamEvents(hook);
  LOG_ICE(LS_INFO) << hook.ToString();
  LOG_ICE(LS_INFO) << event->ToString();
}

void IceLogger::LogConnectionCreated(cricket::Connection* conn) {
  IceConnectionId cnid = RegisterConnection(conn);
  LogEvent* event = CreateLogEventAndAddToEventPool(
      LogEventType(LogEventType::kConnectionCreated));
  event->AddData("connection", *connection_property_by_id_[cnid], true);
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
  event->AddHookForDownstreamEvents(
      LogHook(event, LogEventType::kConnectionReselected));
  LOG_ICE(LS_INFO) << event->ToString();
}

void IceLogger::LogConnectionReselected(cricket::Connection* conn_old,
                                        cricket::Connection* conn_new) {
  // todo(qingsi): may need to discard unnecessary registration
  IceConnectionId cnid_old = RegisterConnection(conn_old);
  IceConnectionId cnid_new = RegisterConnection(conn_new);

  LogEvent* event = CreateLogEventAndAddToEventPool(
      LogEventType(LogEventType::kConnectionReselected));
  event->AddData("old_connection", *connection_property_by_id_[cnid_old], true);
  event->AddData("new_connection", *connection_property_by_id_[cnid_new], true);
  LOG_ICE(LS_INFO) << event->ToString();
}

}  // namespace icelog

}  // namespace webrtc
