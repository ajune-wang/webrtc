/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/network/emulated_network_controller.h"

#include "absl/memory/memory.h"

namespace webrtc {
namespace test {

EmulatedNetworkControllerImpl::EmulatedNetworkControllerImpl(
    Clock* clock,
    std::vector<EmulatedEndpoint*> endpoints)
    : network_manager_(absl::make_unique<EmulatedNetworkManager>(endpoints)),
      socket_server_(
          absl::make_unique<FakeNetworkSocketServer>(clock,
                                                     network_manager_.get())),
      network_thread_(absl::make_unique<rtc::Thread>(socket_server_.get())) {
  network_thread_->SetName("network_thread", nullptr);
  network_thread_->Start();
}
EmulatedNetworkControllerImpl::~EmulatedNetworkControllerImpl() = default;

rtc::Thread* EmulatedNetworkControllerImpl::network_thread() const {
  return network_thread_.get();
}

rtc::NetworkManager* EmulatedNetworkControllerImpl::network_manager() const {
  return network_manager_.get();
}

void EmulatedNetworkControllerImpl::AddEmulatedEndpoint(
    EmulatedEndpoint* endpoint) {
  network_manager_->AddEmulatedEndpoint(endpoint);
}

void EmulatedNetworkControllerImpl::RemoveEmulatedEndpoint(
    EmulatedEndpoint* endpoint) {
  network_manager_->RemoveEmulatedEndpoint(endpoint);
}

}  // namespace test
}  // namespace webrtc
