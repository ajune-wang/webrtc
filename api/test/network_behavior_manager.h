/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETWORK_BEHAVIOR_MANAGER_H_
#define API_TEST_NETWORK_BEHAVIOR_MANAGER_H_

#include <memory>

#include "api/test/simulated_network.h"
#include "rtc_base/socketaddress.h"

namespace webrtc {

class NetworkBehaviorManagerInterface {
 public:
  virtual ~NetworkBehaviorManagerInterface() = default;

  // Create instance of specified network behavior. If it is required
  // to configure created instance later, manager should store
  // references on created instances and expose some method to
  // reconfigure them.
  // |local_address| is local address in the network, which behavior will be
  // emulated with this instance of NetworkBehaviorInterface.
  //
  // Underline network emulation pipeline inside WebRTC requires unique pointer
  // on the network behavior. To be on the safe side implementation can
  // also keep unique pointer and use a proxy, which ownership will be passed
  // to network emulation pipeline. Then implementation can react on destruction
  // of proxy.
  virtual std::unique_ptr<NetworkBehaviorInterface> CreateNetworkBehavior(
      const rtc::SocketAddress& local_address) = 0;
};

}  // namespace webrtc

#endif  // API_TEST_NETWORK_BEHAVIOR_MANAGER_H_
