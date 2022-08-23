/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICE_CONTROLLER_OBSERVER_H_
#define P2P_BASE_ICE_CONTROLLER_OBSERVER_H_

#include "p2p/base/connection.h"
#include "p2p/base/ice_controller_request_types.h"

namespace cricket {

class IceControllerObserver {
 public:
  virtual ~IceControllerObserver() = default;

  // TODO(samvi) these are not the correct data types to supply to the observer
  //   this should change to webrtc::IceCandidateInterface or at the very least
  //   cricket::Candidate.

  virtual void OnConnectionAdded(const Connection* connection) {}
  virtual void OnConnectionReport(const Connection* connection) {}
  virtual void OnConnectionSwitched(const Connection* connection) {}
  virtual void OnConnectionDestroyed(const Connection* connection) {}

  virtual void OnPingRequest(const PingRequest& ping_request) {}
  virtual void OnSwitchRequest(const SwitchRequest& switch_request) {}
  virtual void OnPruneRequest(const PruneRequest& prune_request) {}
};

}  // namespace cricket

#endif  // P2P_BASE_ICE_CONTROLLER_OBSERVER_H_
