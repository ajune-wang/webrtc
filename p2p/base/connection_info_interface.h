/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_CONNECTION_INFO_INTERFACE_H_
#define P2P_BASE_CONNECTION_INFO_INTERFACE_H_

#include <cstdint>

#include "api/candidate.h"
#include "p2p/base/candidate_pair_interface.h"
#include "rtc_base/network.h"

namespace cricket {

class ConnectionInfoInterface : public CandidatePairInterface {
 public:
  virtual ~ConnectionInfoInterface() = default;

  // Implementation of virtual methods in CandidatePairInterface.
  // Returns the description of the local port
  virtual const Candidate& local_candidate() const = 0;

  // Returns the description of the remote port to which we communicate.
  virtual const Candidate& remote_candidate() const = 0;

  // Return local network for this connection.
  virtual const rtc::Network* network() const = 0;

  // Return generation for this connection.
  virtual int generation() const = 0;

  // Returns the pair priority.
  virtual uint64_t priority() const = 0;

  // Return remote nomination.
  virtual uint32_t remote_nomination() const = 0;
};

}  // namespace cricket

#endif  // P2P_BASE_CONNECTION_INFO_INTERFACE_H_
