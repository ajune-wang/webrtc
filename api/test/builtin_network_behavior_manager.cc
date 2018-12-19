/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/builtin_network_behavior_manager.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "call/simulated_network.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {
namespace {

class BuiltInNetworkBehaviorManagerImpl;

class BuiltInNetworkBehaviorManagerImpl : public BuiltInNetworkBehaviorManager {
 public:
  explicit BuiltInNetworkBehaviorManagerImpl(
      const BuiltInNetworkBehaviorConfig& config)
      : BuiltInNetworkBehaviorManager(config) {}

  ~BuiltInNetworkBehaviorManagerImpl() override = default;

  std::unique_ptr<NetworkBehaviorInterface> CreateNetworkBehavior(
      const rtc::SocketAddress& local_address) override {
    auto network = absl::make_unique<SimulatedNetwork>(config_);
    auto network_proxy = absl::make_unique<NetworkBehaviorProxy>(network.get());
    {
      rtc::CritScope crit(&networks_lock_);
      created_networks_.push_back(std::move(network));
    }
    return network_proxy;
  }

  void SetConfig(const BuiltInNetworkBehaviorConfig& config) override {
    config_ = config;
    {
      rtc::CritScope crit(&networks_lock_);
      for (auto& network : created_networks_) {
        network->SetConfig(config);
      }
    }
  }

 private:
  // We need to pass ownership of network behavior to the caller, but also
  // manager have to be sure, that it will reconfigure only alive networks.
  // So manager will keep ownership of the original network behavior. Because
  // manager lifetime is equal to whole test, it means proxy won't be used
  // after manager and origin behavior destruction.
  class NetworkBehaviorProxy : public NetworkBehaviorInterface {
   public:
    explicit NetworkBehaviorProxy(SimulatedNetwork* delegate)
        : delegate_(delegate) {}

    ~NetworkBehaviorProxy() override = default;

    bool EnqueuePacket(PacketInFlightInfo packet_info) override {
      return delegate_->EnqueuePacket(packet_info);
    }

    std::vector<PacketDeliveryInfo> DequeueDeliverablePackets(
        int64_t receive_time_us) override {
      return delegate_->DequeueDeliverablePackets(receive_time_us);
    };

    absl::optional<int64_t> NextDeliveryTimeUs() const override {
      return delegate_->NextDeliveryTimeUs();
    }

   private:
    SimulatedNetwork* delegate_;
  };

  rtc::CriticalSection networks_lock_;
  std::vector<std::unique_ptr<SimulatedNetwork>> created_networks_
      RTC_GUARDED_BY(networks_lock_);
};
}  // namespace

std::unique_ptr<BuiltInNetworkBehaviorManager>
CreateBuiltInNetworkBehaviorManager(BuiltInNetworkBehaviorConfig config) {
  return absl::make_unique<BuiltInNetworkBehaviorManagerImpl>(config);
}
}  // namespace webrtc
