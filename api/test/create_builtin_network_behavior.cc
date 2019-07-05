/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/create_builtin_network_behavior.h"

#include "absl/memory/memory.h"
#include "call/simulated_network.h"

namespace webrtc {

std::unique_ptr<BuiltInNetworkBehavior> CreateBuiltInNetworkBehavior(
    BuiltInNetworkBehaviorConfig config) {
  return absl::make_unique<SimulatedNetwork>(config);
}

}  // namespace webrtc
