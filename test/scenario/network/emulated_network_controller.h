/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_EMULATED_NETWORK_CONTROLLER_H_
#define TEST_SCENARIO_NETWORK_EMULATED_NETWORK_CONTROLLER_H_

#include <memory>
#include <vector>

#include "api/test/network_emulation_manager.h"
#include "rtc_base/async_invoker.h"
#include "rtc_base/thread.h"
#include "test/scenario/network/emulated_network_manager.h"
#include "test/scenario/network/fake_network_socket_server.h"

namespace webrtc {
namespace test {

class EmulatedNetworkControllerImpl : public EmulatedNetworkController {
 public:
  EmulatedNetworkControllerImpl(Clock* clock,
                                std::vector<EmulatedEndpoint*> endpoints);
  ~EmulatedNetworkControllerImpl() override;

  rtc::Thread* network_thread() const override;
  rtc::NetworkManager* network_manager() const override;

  // Provides ability to dynamically change set of endpoint nodes for this
  // controller.
  void AddEmulatedEndpoint(EmulatedEndpoint* endpoint) override;
  void RemoveEmulatedEndpoint(EmulatedEndpoint* endpoint) override;

 private:
  std::unique_ptr<EmulatedNetworkManager> network_manager_;
  std::unique_ptr<FakeNetworkSocketServer> socket_server_;
  std::unique_ptr<rtc::Thread> network_thread_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_EMULATED_NETWORK_CONTROLLER_H_
