/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/network/emulated_network_manager.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"

namespace webrtc {
namespace test {

EmulatedNetworkManager::EmulatedNetworkManager(
    EndpointsController* endpoints_controller,
    std::unique_ptr<rtc::Thread> network_thread)
    : endpoints_controller_(endpoints_controller),
      network_thread_(std::move(network_thread)),
      sent_first_update_(false),
      start_count_(0) {}

void EmulatedNetworkManager::EnableEndpoint(EmulatedEndpoint* endpoint) {
  RTC_CHECK(endpoints_controller_->HasEndpoint(endpoint))
      << "No such interface: " << endpoint->GetPeerLocalAddress().ToString();
  network_thread_->PostTask(RTC_FROM_HERE, [this, endpoint]() {
    endpoint->Enable();
    UpdateNetworksOnce();
  });
}

void EmulatedNetworkManager::DisableEndpoint(EmulatedEndpoint* endpoint) {
  RTC_CHECK(endpoints_controller_->HasEndpoint(endpoint))
      << "No such interface: " << endpoint->GetPeerLocalAddress().ToString();
  network_thread_->PostTask(RTC_FROM_HERE, [this, endpoint]() {
    endpoint->Disable();
    UpdateNetworksOnce();
  });
}

// Network manager interface. All these methods are supposed to be called from
// the same thread.
void EmulatedNetworkManager::StartUpdating() {
  RTC_DCHECK_EQ(network_thread_.get(), rtc::Thread::Current());

  if (start_count_) {
    // If network interfaces are already discovered and signal is sent,
    // we should trigger network signal immediately for the new clients
    // to start allocating ports.
    if (sent_first_update_)
      network_thread_->PostTask(RTC_FROM_HERE,
                                [this]() { MaybeSignalNetworksChanged(); });
  } else {
    network_thread_->PostTask(RTC_FROM_HERE,
                              [this]() { UpdateNetworksOnce(); });
  }
  ++start_count_;
}

void EmulatedNetworkManager::StopUpdating() {
  RTC_DCHECK_EQ(network_thread_.get(), rtc::Thread::Current());
  if (!start_count_)
    return;

  --start_count_;
  if (!start_count_) {
    sent_first_update_ = false;
  }
}

void EmulatedNetworkManager::UpdateNetworksOnce() {
  RTC_DCHECK_EQ(network_thread_.get(), rtc::Thread::Current());

  std::vector<rtc::Network*> networks =
      endpoints_controller_->GetEnabledNetworks();
  for (auto* net : networks) {
    net->set_default_local_address_provider(this);
  }

  bool changed;
  MergeNetworkList(networks, &changed);
  if (changed || !sent_first_update_) {
    MaybeSignalNetworksChanged();
    sent_first_update_ = true;
  }
}

void EmulatedNetworkManager::MaybeSignalNetworksChanged() {
  RTC_DCHECK_EQ(network_thread_.get(), rtc::Thread::Current());
  // If manager is stopped we don't need to signal anything.
  if (start_count_ == 0) {
    return;
  }
  SignalNetworksChanged();
}

}  // namespace test
}  // namespace webrtc
