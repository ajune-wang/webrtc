/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICE_AGENT_INTERFACE_H_
#define P2P_BASE_ICE_AGENT_INTERFACE_H_

#include <vector>

#include "p2p/base/connection.h"
#include "p2p/base/ice_switch_reason.h"

namespace cricket {

// IceAgentInterface provides methods that allow an ICE controller to manipulate
// the connections available to a transport, and used by the transport to
// transfer data.
class IceAgentInterface {
 public:
  virtual ~IceAgentInterface() = default;

  // Called when a pingable connection first becomes available.
  virtual void OnStartedPinging() = 0;

  // Called when the available connections have been reordered, which may lead
  // to transport state changes.
  virtual void OnConnectionsResorted() = 0;

  // Get the time when the last ping was sent.
  virtual int64_t GetLastPingSentMs() const = 0;

  // Return whether this ICE agent is allowed to prune connections. An ICE
  // controller agent can only prune in certain situations.
  virtual bool ShouldPruneConnections() const = 0;

  // Update the state of all available connections.
  virtual void UpdateConnectionStates() = 0;

  // Reset any accumulated state for the given connections.
  virtual void ForgetLearnedStateForConnections(
      std::vector<const Connection*> connections) = 0;

  // Send a STUN ping request for the given connection.
  virtual void SendPingRequest(const Connection* connection) = 0;

  // Switch the transport to use the given connection.
  virtual void SwitchSelectedConnection(const Connection* new_connection,
                                        IceSwitchReason reason) = 0;

  // Prune away the given connections.
  virtual void PruneConnections(std::vector<const Connection*> connections) = 0;
};

}  // namespace cricket

#endif  // P2P_BASE_ICE_AGENT_INTERFACE_H_
