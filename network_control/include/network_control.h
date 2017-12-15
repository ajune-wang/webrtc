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

  TargetTransferRate::Producer* TargetTransferRateProducer;
  PacerConfig::Producer* PacerConfigProducer;
  ProbeClusterConfig::Producer* ProbeClusterConfigProducer;
  CongestionWindow::Producer* CongestionWindowProducer;
};

struct NetworkControlReceivers {
  using uptr = std::unique_ptr<NetworkControlReceivers>;
  // Processing
  ProcessInterval::Receiver* ProcessIntervalReceiver;
  // Estimation
  TransportLossReport::Receiver* TransportLossReportReceiver;
  RoundTripTimeReport::Receiver* RoundTripTimeReportReceiver;
  RemoteBitrateReport::Receiver* RemoteBitrateReportReceiver;
  TransportPacketsFeedback::Receiver* TransportPacketsFeedbackReceiver;
  NetworkRouteChange::Receiver* NetworkRouteChangeReceiver;
  // Control
  SentPacket::Receiver* SentPacketReceiver;
  NetworkAvailability::Receiver* NetworkAvailabilityReceiver;
  TargetRateConstraints::Receiver* TransferRateConstraintsReceiver;
  StreamsConfig::Receiver* StreamsConfigReceiver;
};

class NetworkControllerInterface {
 public:
  virtual ~NetworkControllerInterface() = default;
  virtual NetworkControlReceivers::uptr GetReceivers() = 0;
  virtual NetworkControlProducers::uptr GetProducers() = 0;
  virtual units::TimeDelta GetProcessInterval() = 0;
};
}  // namespace network
}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_CONTROL_H_
