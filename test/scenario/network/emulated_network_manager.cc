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
namespace {

constexpr uint32_t kUpdateNetworksMessage = 1;
constexpr uint32_t kSignalNetworksMessage = 2;

constexpr int kFakeIPv4NetworkPrefixLength = 24;
constexpr int kFakeIPv6NetworkPrefixLength = 64;

}  // namespace

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

void EmulatedNetworkManager::OnMessage(rtc::Message* msg) {
  switch (msg->message_id) {
    case kUpdateNetworksMessage: {
      UpdateNetworksOnce();
      break;
    }
    case kSignalNetworksMessage: {
      SignalNetworksChanged();
      break;
    }
    default:
      RTC_NOTREACHED();
  }
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
      thread_->Post(RTC_FROM_HERE, this, kSignalNetworksMessage);
  } else {
    thread_->Post(RTC_FROM_HERE, this, kUpdateNetworksMessage);
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
    thread_->Clear(this);
    thread_ = nullptr;
    sent_first_update_ = false;
  }
}

bool EmulatedNetworkManager::HasEndpoint(EmulatedEndpoint* endpoint) {
  auto it = absl::c_find_if(endpoints_, [endpoint](EmulatedEndpoint* e) {
    return e->GetId() == endpoint->GetId();
  });
  return it != endpoints_.end();
}

void EmulatedNetworkManager::MaybePostUpdateNetworks() {
  rtc::CritScope crit(&thread_lock_);
  if (thread_ == nullptr) {
    // If thread is null, no need to post update, cause network manager is
    // turned off.
    return;
  }
  thread_->Post(RTC_FROM_HERE, this, kUpdateNetworksMessage);
}

void EmulatedNetworkManager::UpdateNetworksOnce() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<rtc::IPAddress> ips;
  {
    rtc::CritScope crit(&endpoints_lock_);
    for (auto* endpoint : endpoints_) {
      if (!endpoint->is_enabled()) {
        continue;
      }
      ips.push_back(endpoint->GetPeerLocalAddress());
    }
  }

  std::vector<rtc::Network*> networks;
  for (auto& ip : ips) {
    int prefix_length = 0;
    if (ip.family() == AF_INET) {
      prefix_length = kFakeIPv4NetworkPrefixLength;
    } else if (ip.family() == AF_INET6) {
      prefix_length = kFakeIPv6NetworkPrefixLength;
    }
    rtc::IPAddress prefix = TruncateIP(ip, prefix_length);
    std::unique_ptr<rtc::Network> net = absl::make_unique<rtc::Network>(
        ip.ToString(), ip.ToString(), prefix, prefix_length,
        rtc::AdapterType::ADAPTER_TYPE_UNKNOWN);
    net->set_default_local_address_provider(this);
    net->AddIP(ip);
    networks.push_back(net.release());
  }
  bool changed;
  MergeNetworkList(networks, &changed);
  if (changed || !sent_first_update_) {
    SignalNetworksChanged();
    sent_first_update_ = true;
  }
}

}  // namespace test
}  // namespace webrtc
