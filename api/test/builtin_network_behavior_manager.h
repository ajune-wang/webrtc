/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_BUILTIN_NETWORK_BEHAVIOR_MANAGER_H_
#define API_TEST_BUILTIN_NETWORK_BEHAVIOR_MANAGER_H_

#include <memory>

#include "api/test/network_behavior_manager.h"
#include "api/test/simulated_network.h"

namespace webrtc {

class BuiltInNetworkBehaviorManager : public NetworkBehaviorManagerInterface {
 public:
  explicit BuiltInNetworkBehaviorManager(
      const BuiltInNetworkBehaviorConfig& config)
      : config_(config) {}

  ~BuiltInNetworkBehaviorManager() override = default;

  virtual void SetConfig(const BuiltInNetworkBehaviorConfig& config) = 0;

 protected:
  BuiltInNetworkBehaviorConfig config_;
};

std::unique_ptr<BuiltInNetworkBehaviorManager>
CreateBuiltInNetworkBehaviorManager(BuiltInNetworkBehaviorConfig config);

}  // namespace webrtc

#endif  // API_TEST_BUILTIN_NETWORK_BEHAVIOR_MANAGER_H_
