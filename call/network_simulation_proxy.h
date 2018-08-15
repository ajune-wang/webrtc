/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_NETWORK_SIMULATION_PROXY_H_
#define CALL_NETWORK_SIMULATION_PROXY_H_

#include <vector>

#include "api/test/simulated_network.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

// Class acts as proxy for NetworkSimulationInterface and is used to hide from
// FakeNetworkPipethe fact that origin NetworkSimulationInterface instance
// is shared.
class NetworkSimulationProxy : public NetworkSimulationInterface {
 public:
  explicit NetworkSimulationProxy(
      rtc::scoped_refptr<NetworkSimulationInterface> delegate);
  ~NetworkSimulationProxy() override;

  // NetworkSimulationInterface
  bool EnqueuePacket(PacketInFlightInfo packet) override;
  std::vector<PacketDeliveryInfo> DequeueDeliverablePackets(
      int64_t receive_time_us) override;

  absl::optional<int64_t> NextDeliveryTimeUs() const override;

 private:
  rtc::scoped_refptr<NetworkSimulationInterface> delegate_;
};

}  // namespace webrtc

#endif  // CALL_NETWORK_SIMULATION_PROXY_H_
