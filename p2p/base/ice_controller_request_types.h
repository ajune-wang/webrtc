/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICE_CONTROLLER_REQUEST_TYPES_H_
#define P2P_BASE_ICE_CONTROLLER_REQUEST_TYPES_H_

#include <vector>

#include "absl/types/optional.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_recheck_event.h"
#include "p2p/base/ice_switch_reason.h"

namespace cricket {

// TODO(samvi) Migrate cricket::IceControllerInterface::PingResult to this.
// This represents the result of a call to SelectConnectionToPing.
struct PingRequest {
  PingRequest(absl::optional<const Connection*> _connection,
              int _recheck_delay_ms)
      : connection(_connection),
        recheck_delay_ms(_recheck_delay_ms > 0
                             ? absl::make_optional<int>(_recheck_delay_ms)
                             : absl::nullopt) {}

  // Connection that we should (optionally) ping.
  const absl::optional<const Connection*> connection;

  // The delay before another connection is selected to Ping.
  const absl::optional<int> recheck_delay_ms;
};

// TODO(samvi) Migrate cricket::IceControllerInterface::SwitchResult to this.
// This represents the result of a switch call.
struct SwitchRequest {
  // The reason for which this switch was initiated.
  IceSwitchReason reason;

  // Connection that we should (optionally) switch to.
  absl::optional<const Connection*> connection;

  // An optional recheck event for when a Switch() should be attempted again.
  absl::optional<IceRecheckEvent> recheck_event;

  // A vector with connection to run ForgetLearnedState on.
  std::vector<const Connection*> connections_to_forget_state_on;

  // Whether the request can be canceled. A switch may not be canceled if, for
  // instance, it is at the controlled agent because of an indication from the
  // controlling agent.
  // This could possibly be tied to the IceSwitchReason, but stated explicitly
  // for now.
  bool cancelable;
};

// This represents the result of a call to SelectConnectionsToPrune.
struct PruneRequest {
  // A vector with connections to prune.
  std::vector<const Connection*> connections_to_prune;
};

}  // namespace cricket

#endif  // P2P_BASE_ICE_CONTROLLER_REQUEST_TYPES_H_
