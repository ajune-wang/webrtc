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
struct NetworkControlProducers {
  using uptr = std::unique_ptr<NetworkControlProducers>;

  CongestionWindow::Producer* CongestionWindowProducer;
  PacerConfig::Producer* PacerConfigProducer;
  ProbeClusterConfig::Producer* ProbeClusterConfigProducer;
  TargetTransferRate::Producer* TargetTransferRateProducer;
};

struct NetworkControlReceivers {
  using uptr = std::unique_ptr<NetworkControlReceivers>;
  NetworkAvailability::Receiver* NetworkAvailabilityReceiver;
  NetworkRouteChange::Receiver* NetworkRouteChangeReceiver;
  ProcessInterval::Receiver* ProcessIntervalReceiver;
  RemoteBitrateReport::Receiver* RemoteBitrateReportReceiver;
  RoundTripTimeReport::Receiver* RoundTripTimeReportReceiver;
  SentPacket::Receiver* SentPacketReceiver;
  StreamsConfig::Receiver* StreamsConfigReceiver;
  TargetRateConstraints::Receiver* TransferRateConstraintsReceiver;
  TransportLossReport::Receiver* TransportLossReportReceiver;
  TransportPacketsFeedback::Receiver* TransportPacketsFeedbackReceiver;
};

class NetworkControllerInterface {
 public:
  virtual ~NetworkControllerInterface() = default;
  virtual NetworkControlProducers::uptr GetProducers() = 0;
  virtual NetworkControlReceivers::uptr GetReceivers() = 0;
  virtual units::TimeDelta GetProcessInterval() = 0;
};

struct NetworkControlObservers {
  using uptr = std::unique_ptr<NetworkControlObservers>;
  CongestionWindow::Observer* CongestionWindowObserver;
  PacerConfig::Observer* PacerConfigObserver;
  ProbeClusterConfig::Observer* ProbeClusterConfigObserver;
  TargetTransferRate::Observer* TargetTransferRateObserver;
};

struct NetworkControlHandlers {
  using uptr = std::unique_ptr<NetworkControlHandlers>;
  NetworkAvailability::Handler* NetworkAvailabilityHandler;
  NetworkRouteChange::Handler* NetworkRouteChangeHandler;
  ProcessInterval::Handler* ProcessIntervalHandler;
  RemoteBitrateReport::Handler* RemoteBitrateReportHandler;
  RoundTripTimeReport::Handler* RoundTripTimeReportHandler;
  SentPacket::Handler* SentPacketHandler;
  StreamsConfig::Handler* StreamsConfigHandler;
  TargetRateConstraints::Handler* TransferRateConstraintsHandler;
  TransportLossReport::Handler* TransportLossReportHandler;
  TransportPacketsFeedback::Handler* TransportPacketsFeedbackHandler;
};

class NetworkControllerInternalInterface {
 public:
  using uptr = std::unique_ptr<NetworkControllerInternalInterface>;
  virtual ~NetworkControllerInternalInterface() = default;
  virtual units::TimeDelta GetProcessInterval() = 0;
  virtual void ConnectHandlers(NetworkControlHandlers::uptr) = 0;
};
}  // namespace network
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_CONTROL_H_
