/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICELOGGER_H_
#define P2P_BASE_ICELOGGER_H_

#include <map>
#include <memory>
#include <deque>
#include <string>

#include "p2p/base/icelogtype.h"
#include "rtc_base/json.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace icelog {

class IceLogger {
 public:
  IceLogger();
  static std::unique_ptr<IceLogger> Create();

  IceCandidateId RegisterCandidate(cricket::Port* port,
                                   const cricket::Candidate& c,
                                   bool is_remote);
  IceConnectionId RegisterConnection(cricket::Connection* conn);
  void LogCandidateGathered(cricket::Port* port, const cricket::Candidate& c);
  void LogConnectionPingResponseReceived(cricket::Connection* conn);
  void LogConnectionReselected(cricket::Connection* conn_old,
                               cricket::Connection* conn_new);

 private:
  static const char ice_log_header[];
  std::map<IceCandidateId, std::unique_ptr<IceCandidateProperty>> candidates_;
  std::map<IceConnectionId, std::unique_ptr<IceConnectionProperty>>
      connections_;
  // todo(qingsi) IceEvent definition and identifier-to-event definition of
  // event queues
  std::deque<std::string> events_;
};

}  // namespace icelog

}  // namespace webrtc

#endif  // P2P_BASE_ICELOGGER_H_
