/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_CONTROL_INCLUDE_NETWORK_CONTROLLER_H_
#define NETWORK_CONTROL_INCLUDE_NETWORK_CONTROLLER_H_
#include <stdint.h>
#include <memory>
#include <string>

#include "rtc_base/task_queue.h"

#include "network_control/include/network_control.h"

namespace webrtc {
namespace network {
namespace internal {
class NetworkControlJunctions : public NetworkControlProducer,
                                public NetworkControlObserver,
                                public CongestionWindow::SimpleJunction,
                                public PacerConfig::SimpleJunction,
                                public ProbeClusterConfig::SimpleJunction,
                                public TargetTransferRate::SimpleJunction {
 public:
  using uptr = std::unique_ptr<NetworkControlJunctions>;
  NetworkControlJunctions();
  ~NetworkControlJunctions() override;
  RTC_DISALLOW_COPY_AND_ASSIGN(NetworkControlJunctions);
};

class NetworkInformationJunctions
    : public virtual NetworkInformationReceiver,
      public virtual NetworkInformationProducer,
      public NetworkAvailability::SimpleJunction,
      public NetworkRouteChange::SimpleJunction,
      public ProcessInterval::SimpleJunction,
      public RemoteBitrateReport::SimpleJunction,
      public RoundTripTimeReport::SimpleJunction,
      public SentPacket::SimpleJunction,
      public StreamsConfig::SimpleJunction,
      public TargetRateConstraints::SimpleJunction,
      public TransportLossReport::SimpleJunction,
      public TransportPacketsFeedback::SimpleJunction {
 public:
  using uptr = std::unique_ptr<NetworkInformationJunctions>;

  NetworkInformationJunctions();
  ~NetworkInformationJunctions() override;
};
}  // namespace internal

// This class exist mainly to ensure safe destruction and construction, it also
// works as a wrapper for network controllers to provide access to Observers and
// Producers
//
// The order of the members is important, the junctions given to a controller
// must outlive the controller, while the controller must outlive the receivers.
// This way it's always safe for a controller to produce and it can never
// receive
class NetworkControllerWrapper {
 public:
  explicit NetworkControllerWrapper(
      NetworkControllerFactoryInterface::sptr controller_factory);
  ~NetworkControllerWrapper();

  NetworkControlProducer* GetProducers();
  NetworkInformationReceiver* GetReceivers();
  units::TimeDelta GetProcessInterval();

 private:
  // The ordering of theese protects against calls on destructed objects.
  internal::NetworkControlJunctions::uptr junctions_;
  NetworkControllerInterface::uptr controller_;
  internal::NetworkInformationJunctions::uptr receivers_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(NetworkControllerWrapper);
};
}  // namespace network
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_CONTROLLER_H_
