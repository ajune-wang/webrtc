/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/network_simulation_proxy.h"

namespace webrtc {

NetworkSimulationProxy::NetworkSimulationProxy(
    rtc::scoped_refptr<NetworkSimulationInterface> delegate)
    : delegate_(delegate) {}

NetworkSimulationProxy::~NetworkSimulationProxy() = default;

bool NetworkSimulationProxy::EnqueuePacket(PacketInFlightInfo packet) {
  return delegate_.EnqueuePacket(packet);
}

absl::optional<int64_t> NetworkSimulationProxy::NextDeliveryTimeUs() const {
  return delegate_.NextDeliveryTimeUs();
}

std::vector<PacketDeliveryInfo>
NetworkSimulationProxy::DequeueDeliverablePackets(int64_t receive_time_us) {
  return delegate_.DequeueDeliverablePackets(receive_time_us);
}

}  // namespace webrtc
