/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_CONTROL_INCLUDE_NETWORK_CONTROL_H_
#define NETWORK_CONTROL_INCLUDE_NETWORK_CONTROL_H_
#include <stdint.h>
#include <memory>

#include "network_control/include/network_types.h"
#include "network_control/include/network_units.h"

namespace webrtc {

class NetworkControllerObserver {
 public:
  virtual void OnCongestionWindow(CongestionWindow) = 0;
  virtual void OnPacerConfig(PacerConfig) = 0;
  virtual void OnProbeClusterConfig(ProbeClusterConfig) = 0;
  virtual void OnTargetTransferRate(TargetTransferRate) = 0;

 protected:
  virtual ~NetworkControllerObserver() = default;
};

class NetworkControllerInterface {
 public:
  using uptr = std::unique_ptr<NetworkControllerInterface>;
  virtual ~NetworkControllerInterface() = default;
  virtual TimeDelta GetProcessInterval() = 0;

  virtual void OnNetworkAvailability(NetworkAvailability) = 0;
  virtual void OnNetworkRouteChange(NetworkRouteChange) = 0;
  virtual void OnProcessInterval(ProcessInterval) = 0;
  virtual void OnRemoteBitrateReport(RemoteBitrateReport) = 0;
  virtual void OnRoundTripTimeReport(RoundTripTimeReport) = 0;
  virtual void OnSentPacket(SentPacket) = 0;
  virtual void OnStreamsConfig(StreamsConfig) = 0;
  virtual void OnTargetRateConstraints(TargetRateConstraints) = 0;
  virtual void OnTransportLossReport(TransportLossReport) = 0;
  virtual void OnTransportPacketsFeedback(TransportPacketsFeedback) = 0;
};

class NetworkControllerFactoryInterface {
 public:
  using uptr = std::unique_ptr<NetworkControllerFactoryInterface>;
  virtual NetworkControllerInterface::uptr Create(
      NetworkControllerObserver* observer) = 0;
  virtual ~NetworkControllerFactoryInterface() = default;
};
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_CONTROL_H_
