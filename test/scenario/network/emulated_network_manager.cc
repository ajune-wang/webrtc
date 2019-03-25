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

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"

namespace webrtc {
namespace test {

EmulatedNetworkManager::EmulatedNetworkManager(
    std::vector<EmulatedEndpoint*> endpoints)
    : thread_(nullptr),
      sent_first_update_(false),
      start_count_(0),
      endpoints_(std::move(endpoints)) {}

EmulatedEndpoint* EmulatedNetworkManager::GetEndpointNode(
    const rtc::IPAddress& ip) const {
  rtc::CritScope crit(&endpoints_lock_);
  for (auto* endpoint : endpoints_) {
    rtc::IPAddress peerLocalAddress = endpoint->GetPeerLocalAddress();
    if (peerLocalAddress == ip) {
      return endpoint;
    }
  }
  RTC_CHECK(false) << "No network found for address" << ip.ToString();
}

void EmulatedNetworkManager::EnableEndpoint(EmulatedEndpoint* endpoint) {
  {
    rtc::CritScope crit(&endpoints_lock_);
    RTC_CHECK(HasEndpoint(endpoint))
        << "No such interface: " << endpoint->GetPeerLocalAddress().ToString();
    endpoint->Enable();
  }
  MaybePostUpdateNetworks();
}

void EmulatedNetworkManager::DisableEndpoint(EmulatedEndpoint* endpoint) {
  {
    rtc::CritScope crit(&endpoints_lock_);
    RTC_CHECK(HasEndpoint(endpoint))
        << "No such interface: " << endpoint->GetPeerLocalAddress().ToString();
    endpoint->Disable();
  }
  MaybePostUpdateNetworks();
}

// Network manager interface. All these methods are supposed to be called from
// the same thread.
void EmulatedNetworkManager::StartUpdating() {
  rtc::CritScope crit(&thread_lock_);
  if (thread_ == nullptr) {
    // If thread is null reattach thread checker to the current thread.
    thread_checker_.DetachFromThread();
  }
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  thread_ = rtc::Thread::Current();

  if (start_count_) {
    // If network interfaces are already discovered and signal is sent,
    // we should trigger network signal immediately for the new clients
    // to start allocating ports.
    if (sent_first_update_)
      thread_->PostTask(RTC_FROM_HERE,
                        [this]() { MaybeSignalNetworksChanged(); });
  } else {
    thread_->PostTask(RTC_FROM_HERE, [this]() { UpdateNetworksOnce(); });
  }
  ++start_count_;
}

void EmulatedNetworkManager::StopUpdating() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!start_count_)
    return;

  --start_count_;
  if (!start_count_) {
    rtc::CritScope crit(&thread_lock_);
    thread_ = nullptr;
    sent_first_update_ = false;
  }
}

bool EmulatedNetworkManager::HasEndpoint(EmulatedEndpoint* endpoint) {
  for (auto* e : endpoints_) {
    if (e->GetId() == endpoint->GetId()) {
      return true;
    }
  }
  return false;
}

void EmulatedNetworkManager::MaybePostUpdateNetworks() {
  rtc::CritScope crit(&thread_lock_);
  if (thread_ == nullptr) {
    // If thread is null, no need to post update, cause network manager is
    // turned off.
    return;
  }
  thread_->PostTask(RTC_FROM_HERE, [this]() { UpdateNetworksOnce(); });
}

void EmulatedNetworkManager::UpdateNetworksOnce() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<rtc::Network*> networks;
  {
    rtc::CritScope crit(&endpoints_lock_);
    for (auto* endpoint : endpoints_) {
      if (!endpoint->Enabled()) {
        continue;
      }
      auto net = absl::make_unique<rtc::Network>(endpoint->network());
      net->set_default_local_address_provider(this);
      networks.push_back(net.release());
    }
  }
  bool changed;
  MergeNetworkList(networks, &changed);
  if (changed || !sent_first_update_) {
    MaybeSignalNetworksChanged();
    sent_first_update_ = true;
  }
}

void EmulatedNetworkManager::MaybeSignalNetworksChanged() {
  // If manager is stopped we don't need to signal anything.
  if (start_count_ == 0) {
    return;
  }
  SignalNetworksChanged();
}

}  // namespace test
}  // namespace webrtc
