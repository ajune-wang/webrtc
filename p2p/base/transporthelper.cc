/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/transporthelper.h"

#include <memory>
#include <utility>  // for std::pair

#include "api/candidate.h"
#include "p2p/base/dtlstransport.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/p2ptransportchannel.h"
#include "p2p/base/port.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

using webrtc::SdpType;

namespace cricket {

ConnectionInfo::ConnectionInfo()
    : best_connection(false),
      writable(false),
      receiving(false),
      timeout(false),
      new_connection(false),
      rtt(0),
      sent_total_bytes(0),
      sent_bytes_second(0),
      sent_discarded_packets(0),
      sent_total_packets(0),
      sent_ping_requests_total(0),
      sent_ping_requests_before_first_response(0),
      sent_ping_responses(0),
      recv_total_bytes(0),
      recv_bytes_second(0),
      recv_ping_requests(0),
      recv_ping_responses(0),
      key(nullptr),
      state(IceCandidatePairState::WAITING),
      priority(0),
      nominated(false),
      total_round_trip_time_ms(0) {}

ConnectionInfo::ConnectionInfo(const ConnectionInfo&) = default;

ConnectionInfo::~ConnectionInfo() = default;

TransportChannelStats::TransportChannelStats() = default;

TransportChannelStats::TransportChannelStats(const TransportChannelStats&) =
    default;

TransportChannelStats::~TransportChannelStats() = default;

TransportStats::TransportStats() = default;

TransportStats::~TransportStats() = default;

IceConfig::IceConfig() = default;

IceConfig::IceConfig(int receiving_timeout_ms,
                     int backup_connection_ping_interval,
                     ContinualGatheringPolicy gathering_policy,
                     bool prioritize_most_likely_candidate_pairs,
                     int stable_writable_connection_ping_interval_ms,
                     bool presume_writable_when_fully_relayed,
                     int regather_on_failed_networks_interval_ms,
                     int receiving_switching_delay_ms)
    : receiving_timeout(receiving_timeout_ms),
      backup_connection_ping_interval(backup_connection_ping_interval),
      continual_gathering_policy(gathering_policy),
      prioritize_most_likely_candidate_pairs(
          prioritize_most_likely_candidate_pairs),
      stable_writable_connection_ping_interval(
          stable_writable_connection_ping_interval_ms),
      presume_writable_when_fully_relayed(presume_writable_when_fully_relayed),
      regather_on_failed_networks_interval(
          regather_on_failed_networks_interval_ms),
      receiving_switching_delay(receiving_switching_delay_ms) {}

IceConfig::~IceConfig() = default;

bool BadTransportDescription(const std::string& desc, std::string* err_desc) {
  if (err_desc) {
    *err_desc = desc;
  }
  RTC_LOG(LS_ERROR) << desc;
  return false;
}

bool IceCredentialsChanged(const std::string& old_ufrag,
                           const std::string& old_pwd,
                           const std::string& new_ufrag,
                           const std::string& new_pwd) {
  // The standard (RFC 5245 Section 9.1.1.1) says that ICE restarts MUST change
  // both the ufrag and password. However, section 9.2.1.1 says changing the
  // ufrag OR password indicates an ICE restart. So, to keep compatibility with
  // endpoints that only change one, we'll treat this as an ICE restart.
  return (old_ufrag != new_ufrag) || (old_pwd != new_pwd);
}

bool VerifyCandidate(const Candidate& cand, std::string* error) {
  // No address zero.
  if (cand.address().IsNil() || cand.address().IsAnyIP()) {
    *error = "candidate has address of zero";
    return false;
  }

  // Disallow all ports below 1024, except for 80 and 443 on public addresses.
  int port = cand.address().port();
  if (cand.protocol() == TCP_PROTOCOL_NAME &&
      (cand.tcptype() == TCPTYPE_ACTIVE_STR || port == 0)) {
    // Expected for active-only candidates per
    // http://tools.ietf.org/html/rfc6544#section-4.5 so no error.
    // Libjingle clients emit port 0, in "active" mode.
    return true;
  }
  if (port < 1024) {
    if ((port != 80) && (port != 443)) {
      *error = "candidate has port below 1024, but not 80 or 443";
      return false;
    }

    if (cand.address().IsPrivateIP()) {
      *error = "candidate has port of 80 or 443 with private IP address";
      return false;
    }
  }

  return true;
}

bool VerifyCandidates(const Candidates& candidates, std::string* error) {
  for (const Candidate& candidate : candidates) {
    if (!VerifyCandidate(candidate, error)) {
      return false;
    }
  }
  return true;
}

}  // namespace cricket
