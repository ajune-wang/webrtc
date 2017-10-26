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
IceLogger::IceLogger() {
  connections_[kNullIceConnectionId].reset(new IceConnectionProperty());
}

std::unique_ptr<IceLogger> IceLogger::Create() {
  return rtc::MakeUnique<IceLogger>();
}

IceCandidateId IceLogger::RegisterCandidate(cricket::Port* port,
                                            const cricket::Candidate& c,
                                            bool is_remote) {
  IceCandidateId cid(c.id());
  if (candidates_.find(cid) == candidates_.end()) {
    candidates_[cid].reset(new IceCandidateProperty(*port, c));
    candidates_[cid]->set_is_remote(is_remote);
    LOG_ICE(LS_INFO) << "{\"message\": \"candidate registration\", \"data\": "
                     << candidates_[cid]->ToString() << "}";
  }
  return cid;
}

IceConnectionId IceLogger::RegisterConnection(cricket::Connection* conn) {
  if (conn == nullptr) {
    return kNullIceConnectionId;
  }
  IceConnectionId cnid(conn);
  if (connections_.find(cnid) != connections_.end()) {
    return cnid;
  }
  cricket::Port* local_port = conn->port();
  const cricket::Candidate& local_candidate = conn->local_candidate();
  const IceCandidateId local_candidate_id =
      RegisterCandidate(local_port, local_candidate, false);
  const cricket::Candidate& remote_candidate = conn->remote_candidate();
  const IceCandidateId remote_candidate_id =
      RegisterCandidate(local_port, remote_candidate, true);
  connections_[cnid].reset(new IceConnectionProperty(
      *candidates_[local_candidate_id], *candidates_[remote_candidate_id]));
  LOG_ICE(LS_INFO) << "{\"message\": \"connection registration\", \"data\": "
                   << connections_[cnid]->ToString() << "}";
  return cnid;
}

void IceLogger::LogCandidateGathered(cricket::Port* port,
                                     const cricket::Candidate& c) {
  RegisterCandidate(port, c, false);
}
void IceLogger::LogConnectionPingResponseReceived(cricket::Connection* conn) {
  IceConnectionId cnid(conn);
  if (connections_.find(cnid) == connections_.end()) {
    RegisterConnection(conn);
  }
  std::string event_msg =
      "{\"message\": \"connection ping received\", \"data\": " +
      connections_[cnid]->ToString() + "}";

  LOG_ICE(LS_INFO) << event_msg;
  events_.push_back(event_msg);
}

void IceLogger::LogConnectionReselected(cricket::Connection* conn_old,
                                        cricket::Connection* conn_new) {
  // todo(qingsi): may need to discard unnecessary registration
  IceConnectionId cnid_old = RegisterConnection(conn_old);
  IceConnectionId cnid_new = RegisterConnection(conn_new);
  // todo(qingsi): define IceEvent using IceObject and refactor the manual
  // formatting below
  LOG_ICE(LS_INFO) << "{\"message\": \"connection reselection\", \"data\": "
                   << "{\"old\": " << connections_[cnid_old]->ToString() + ", "
                   << "\"new\": " << connections_[cnid_new]->ToString() << "}}";
  if (cnid_old == kNullIceConnectionId) {
    LOG_ICE(LS_INFO)
        << "{\"message\": \"plain\", \"data\": "
        << "\"Note the old selection is NULL. "
        << "This is likely an initial establishment of connection.\"}";
  }
  if (!events_.empty()) {
    LOG_ICE(LS_INFO)
        << "{\"message\": \"plain\", \"data\": "
        << "\"The reselection is likely triggered by the event:\"}";
    LOG_ICE(LS_INFO) << events_.back();
    LOG_ICE(LS_INFO) << "{\"message\": \"plain\", \"data\": "
                     << "\"The recent events are:\"}";
    int i = 0;
    // todo(qingsi) hardcoded naive backtracing of events
    for (auto it = events_.rbegin(); it != events_.rend() && i < 5; it++, i++) {
      LOG_ICE(LS_INFO) << *it;
    }
  }
}

}  // namespace icelog

}  // namespace webrtc
