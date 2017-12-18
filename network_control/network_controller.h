/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_CONTROL_NETWORK_CONTROLLER_H_
#define NETWORK_CONTROL_NETWORK_CONTROLLER_H_
#include <stdint.h>
#include <memory>
#include <string>

#include "rtc_base/task_queue.h"

#include "network_control/include/network_control.h"

namespace webrtc {
namespace network {

class NetworkControlJunctions {
 public:
  using uptr = std::unique_ptr<NetworkControlJunctions>;
  NetworkControlJunctions();
  ~NetworkControlJunctions();
  CongestionWindow::Junction CongestionWindowJunction;
  PacerConfig::Junction PacerConfigJunction;
  ProbeClusterConfig::Junction ProbeClusterConfigJunction;
  TargetTransferRate::Junction TargetTransferRateJunction;

  NetworkControlObservers::uptr GetObservers();
  NetworkControlProducers::uptr GetProducers();
  RTC_DISALLOW_COPY_AND_ASSIGN(NetworkControlJunctions);
};

class NetworkControlHandlingReceivers {
 public:
  using uptr = std::unique_ptr<NetworkControlHandlingReceivers>;

  explicit NetworkControlHandlingReceivers(
      NetworkControllerInternalInterface* controller);
  explicit NetworkControlHandlingReceivers(
      rtc::TaskQueue* task_queue,
      NetworkControllerInternalInterface* controller);
  ~NetworkControlHandlingReceivers();

  NetworkAvailability::HandlingReceiver::uptr NetworkAvailabilityReceiver;
  NetworkRouteChange::HandlingReceiver::uptr NetworkRouteChangeReceiver;
  ProcessInterval::HandlingReceiver::uptr ProcessIntervalReceiver;
  RemoteBitrateReport::HandlingReceiver::uptr RemoteBitrateReportReceiver;
  RoundTripTimeReport::HandlingReceiver::uptr RoundTripTimeReportReceiver;
  SentPacket::HandlingReceiver::uptr SentPacketReceiver;
  StreamsConfig::HandlingReceiver::uptr StreamsConfigReceiver;
  TargetRateConstraints::HandlingReceiver::uptr TransferRateConstraintsReceiver;
  TransportLossReport::HandlingReceiver::uptr TransportLossReportReceiver;
  TransportPacketsFeedback::HandlingReceiver::uptr
      TransportPacketsFeedbackReceiver;

  NetworkControlHandlers::uptr GetHandlers();
  NetworkControlReceivers::uptr GetReceivers();

 private:
  rtc::CriticalSection lock_;

  RTC_DISALLOW_COPY_AND_ASSIGN(NetworkControlHandlingReceivers);
};

// This class exist mainly to ensure safe destruction and construction, it also
// works as a wrapper for network controllers to provide access to Observers and
// Producers
//
// The order of the members is important, the junctions given to a controller
// must outlive the controller, while the controller must outlive the receivers.
// This way it's always safe for a controller to produce and it can never
// receive
class NetworkControllerWrapper : public NetworkControllerInterface {
 public:
  NetworkControllerWrapper(NetworkControlJunctions::uptr junctions,
                           NetworkControllerInternalInterface::uptr controller,
                           NetworkControlHandlingReceivers::uptr receivers);
  ~NetworkControllerWrapper() override;

  NetworkControlProducers::uptr GetProducers() override;
  NetworkControlReceivers::uptr GetReceivers() override;
  units::TimeDelta GetProcessInterval() override;

 private:
  // The ordering of theese protects against calls on destructed objects.
  NetworkControlJunctions::uptr junctions_;
  NetworkControllerInternalInterface::uptr controller_;
  NetworkControlHandlingReceivers::uptr receivers_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(NetworkControllerWrapper);
};
}  // namespace network
}  // namespace webrtc

#endif  // NETWORK_CONTROL_NETWORK_CONTROLLER_H_
