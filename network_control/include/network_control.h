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

namespace network {
class NetworkControlProducer : public virtual CongestionWindow::Producer,
                               public virtual PacerConfig::Producer,
                               public virtual ProbeClusterConfig::Producer,
                               public virtual TargetTransferRate::Producer {
 protected:
  ~NetworkControlProducer() = default;
};

class NetworkControlObserver : public virtual CongestionWindow::Observer,
                               public virtual PacerConfig::Observer,
                               public virtual ProbeClusterConfig::Observer,
                               public virtual TargetTransferRate::Observer {
 protected:
  ~NetworkControlObserver() = default;
};

class NetworkInformationReceiver
    : public virtual NetworkAvailability::Receiver,
      public virtual NetworkRouteChange::Receiver,
      public virtual ProcessInterval::Receiver,
      public virtual RemoteBitrateReport::Receiver,
      public virtual RoundTripTimeReport::Receiver,
      public virtual SentPacket::Receiver,
      public virtual StreamsConfig::Receiver,
      public virtual TargetRateConstraints::Receiver,
      public virtual TransportLossReport::Receiver,
      public virtual TransportPacketsFeedback::Receiver {
 protected:
  ~NetworkInformationReceiver() = default;
};

class NetworkInformationProducer
    : public virtual NetworkAvailability::Producer,
      public virtual NetworkRouteChange::Producer,
      public virtual ProcessInterval::Producer,
      public virtual RemoteBitrateReport::Producer,
      public virtual RoundTripTimeReport::Producer,
      public virtual SentPacket::Producer,
      public virtual StreamsConfig::Producer,
      public virtual TargetRateConstraints::Producer,
      public virtual TransportLossReport::Producer,
      public virtual TransportPacketsFeedback::Producer {
 protected:
  ~NetworkInformationProducer() = default;
};

class NetworkControllerInterface {
 public:
  using uptr = std::unique_ptr<NetworkControllerInterface>;
  virtual ~NetworkControllerInterface() = default;
  virtual units::TimeDelta GetProcessInterval() = 0;
  virtual void ConnectHandlers(NetworkInformationProducer*) = 0;
};

class NetworkControllerFactoryInterface {
 public:
  using sptr = std::shared_ptr<NetworkControllerFactoryInterface>;
  virtual NetworkControllerInterface::uptr Create(
      NetworkControlObserver* observers) = 0;
  virtual ~NetworkControllerFactoryInterface() = default;
};
}  // namespace network
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_CONTROL_H_
