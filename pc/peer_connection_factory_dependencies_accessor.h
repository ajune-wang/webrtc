/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_PEER_CONNECTION_FACTORY_DEPENDENCIES_ACCESSOR_H_
#define PC_PEER_CONNECTION_FACTORY_DEPENDENCIES_ACCESSOR_H_

#include "absl/base/attributes.h"
#include "api/peer_connection_interface.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class PeerConnectionFactoryDependenciesAccessor final {
 public:
  PeerConnectionFactoryDependenciesAccessor(
      PeerConnectionFactoryDependencies& deps ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : deps_(deps) {}

  PeerConnectionFactoryDependenciesAccessor(
      const PeerConnectionFactoryDependenciesAccessor&) = delete;
  PeerConnectionFactoryDependenciesAccessor& operator=(
      PeerConnectionFactoryDependenciesAccessor&) = delete;

  ~PeerConnectionFactoryDependenciesAccessor() = default;

  Clock* GetClock() { return deps_.clock; }

  PeerConnectionFactoryDependenciesAccessor& Set(Clock* clock) {
    deps_.clock = clock;
    return *this;
  }

 private:
  PeerConnectionFactoryDependencies& deps_;
};

}  // namespace webrtc

#endif  // PC_PEER_CONNECTION_FACTORY_DEPENDENCIES_ACCESSOR_H_
