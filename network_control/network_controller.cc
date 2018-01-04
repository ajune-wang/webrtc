/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "network_control/include/network_controller.h"

#include <algorithm>
#include <functional>
#include <utility>
using rtc::MakeUnique;
namespace webrtc {
namespace network {

namespace internal {
NetworkControlJunctions::NetworkControlJunctions() {}
NetworkControlJunctions::~NetworkControlJunctions() {
  CongestionWindow::SimpleJunction::Disconnect();
  PacerConfig::SimpleJunction::Disconnect();
  ProbeClusterConfig::SimpleJunction::Disconnect();
  TargetTransferRate::SimpleJunction::Disconnect();
}

NetworkInformationJunctions::NetworkInformationJunctions() {}
NetworkInformationJunctions::~NetworkInformationJunctions() {
  NetworkAvailability::SimpleJunction::Disconnect();
  NetworkRouteChange::SimpleJunction::Disconnect();
  ProcessInterval::SimpleJunction::Disconnect();
  RemoteBitrateReport::SimpleJunction::Disconnect();
  RoundTripTimeReport::SimpleJunction::Disconnect();
  SentPacket::SimpleJunction::Disconnect();
  StreamsConfig::SimpleJunction::Disconnect();
  TargetRateConstraints::SimpleJunction::Disconnect();
  TransportLossReport::SimpleJunction::Disconnect();
  TransportPacketsFeedback::SimpleJunction::Disconnect();
}

}  // namespace internal
NetworkControllerWrapper::NetworkControllerWrapper(
    NetworkControllerFactoryInterface::sptr controller_factory)
    : junctions_(MakeUnique<internal::NetworkControlJunctions>()),
      controller_(controller_factory->Create(junctions_.get())),
      receivers_(MakeUnique<internal::NetworkInformationJunctions>()) {
  controller_->ConnectHandlers(receivers_.get());
}

NetworkControllerWrapper::~NetworkControllerWrapper() {}

NetworkControlProducer* NetworkControllerWrapper::GetProducers() {
  return junctions_.get();
}

NetworkInformationReceiver* NetworkControllerWrapper::GetReceivers() {
  return receivers_.get();
}

units::TimeDelta NetworkControllerWrapper::GetProcessInterval() {
  return controller_->GetProcessInterval();
}
}  // namespace network
}  // namespace webrtc
