/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
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

class NetworkControlProducers
    : public virtual CongestionWindow::ProducerConnector,
      public virtual PacerConfig::ProducerConnector,
      public virtual ProbeClusterConfig::ProducerConnector,
      public virtual TargetTransferRate::ProducerConnector {};

class NetworkInformationReceivers
    : public virtual NetworkAvailability::ReceiverConnector,
      public virtual NetworkRouteChange::ReceiverConnector,
      public virtual ProcessInterval::ReceiverConnector,
      public virtual RemoteBitrateReport::ReceiverConnector,
      public virtual RoundTripTimeReport::ReceiverConnector,
      public virtual SentPacket::ReceiverConnector,
      public virtual StreamsConfig::ReceiverConnector,
      public virtual TargetRateConstraints::ReceiverConnector,
      public virtual TransportLossReport::ReceiverConnector,
      public virtual TransportPacketsFeedback::ReceiverConnector {};

class NetworkControllerInterface {
 public:
  using uptr = std::unique_ptr<NetworkControllerInterface>;
  virtual ~NetworkControllerInterface() = default;
  virtual TimeDelta GetProcessInterval() = 0;
  virtual NetworkInformationReceivers GetReceivers() = 0;
  virtual NetworkControlProducers GetProducers() = 0;
};

class NetworkControllerFactoryInterface {
 public:
  using sptr = std::shared_ptr<NetworkControllerFactoryInterface>;
  virtual NetworkControllerInterface::uptr Create() = 0;
  virtual ~NetworkControllerFactoryInterface() = default;
};
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_CONTROL_H_
