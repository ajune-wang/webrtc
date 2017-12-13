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
struct NetworkInformationHandlers {
  using uptr = std::unique_ptr<NetworkInformationHandlers>;
  SentPacket::Handler* SentPacketHandler;
  TransportPacketsFeedback::Handler* TransportPacketsFeedbackHandler;
  TransportLossReport::Handler* TransportLossReportHandler;
  RoundTripTimeReport::Handler* RoundTripTimeReportHandler;
  RemoteBitrateReport::Handler* RemoteBitrateReportHandler;
  TargetRateConstraints::Handler* TransferRateConstraintsHandler;
  StreamsConfig::Handler* StreamsConfigHandler;
  NetworkAvailability::Handler* NetworkAvailabilityHandler;
  NetworkRouteChange::Handler* NetworkRouteChangeHandler;
  ProcessInterval::Handler* ProcessIntervalHandler;
};

class NetworkControllerInternalInterface {
 public:
  using uptr = std::unique_ptr<NetworkControllerInternalInterface>;
  NetworkControllerInternalInterface() = default;
  virtual ~NetworkControllerInternalInterface() {}
  virtual units::TimeDelta GetProcessInterval() = 0;
  virtual NetworkControlProducers::uptr GetProducers() = 0;
  virtual void ConnectHandlers(NetworkInformationHandlers::uptr) = 0;
};

class NetworkControlJunctions {
 public:
  using uptr = std::unique_ptr<NetworkControlJunctions>;
  NetworkControlJunctions();
  ~NetworkControlJunctions();
  // NetworkEstimate::Junction NetworkEstimateJunction;
  TargetTransferRate::Junction TargetTransferRateJunction;
  PacerConfig::Junction PacerConfigJunction;
  CongestionWindow::Junction CongestionWindowJunction;
  ProbeClusterConfig::Junction ProbeClusterConfigJunction;

  NetworkControlProducers::uptr GetProducers();
  RTC_DISALLOW_COPY_AND_ASSIGN(NetworkControlJunctions);
};

class NetworkControlHandlingReceivers {
 public:
  using uptr = std::unique_ptr<NetworkControlHandlingReceivers>;

  virtual ~NetworkControlHandlingReceivers();

  SentPacket::HandlingReceiver::uptr SentPacketReceiver;
  TransportPacketsFeedback::HandlingReceiver::uptr
      TransportPacketsFeedbackReceiver;
  TransportLossReport::HandlingReceiver::uptr TransportLossReportReceiver;
  RoundTripTimeReport::HandlingReceiver::uptr RoundTripTimeReportReceiver;
  RemoteBitrateReport::HandlingReceiver::uptr RemoteBitrateReportReceiver;
  TargetRateConstraints::HandlingReceiver::uptr TransferRateConstraintsReceiver;
  StreamsConfig::HandlingReceiver::uptr StreamsConfigReceiver;
  NetworkAvailability::HandlingReceiver::uptr NetworkAvailabilityReceiver;
  NetworkRouteChange::HandlingReceiver::uptr NetworkRouteChangeReceiver;
  ProcessInterval::HandlingReceiver::uptr ProcessIntervalReceiver;

  NetworkControlReceivers::uptr GetReceivers();

 protected:
  NetworkInformationHandlers::uptr GetHandlers();
  NetworkControlHandlingReceivers();

  RTC_DISALLOW_COPY_AND_ASSIGN(NetworkControlHandlingReceivers);
};

class TaskQueueNetworkControlReceivers
    : public NetworkControlHandlingReceivers {
 public:
  using uptr = std::unique_ptr<TaskQueueNetworkControlReceivers>;
  explicit TaskQueueNetworkControlReceivers(
      rtc::TaskQueue* task_queue,
      NetworkControllerInternalInterface* controller);
  ~TaskQueueNetworkControlReceivers() override;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(TaskQueueNetworkControlReceivers);
};

class LockedNetworkControlReceivers : public NetworkControlHandlingReceivers {
 public:
  using uptr = std::unique_ptr<TaskQueueNetworkControlReceivers>;
  explicit LockedNetworkControlReceivers(
      NetworkControllerInternalInterface* controller);
  ~LockedNetworkControlReceivers() override;

 private:
  rtc::CriticalSection lock_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(LockedNetworkControlReceivers);
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
  NetworkControllerWrapper(NetworkControllerInternalInterface::uptr controller,
                           NetworkControlHandlingReceivers::uptr receivers);
  ~NetworkControllerWrapper() override;

  NetworkControlReceivers::uptr GetReceivers() override;
  NetworkControlProducers::uptr GetProducers() override;
  units::TimeDelta GetProcessInterval() override;

 private:
  // The ordering of theese protects against calls on destructed objects.
  NetworkControllerInternalInterface::uptr controller_;
  NetworkControlHandlingReceivers::uptr receivers_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(NetworkControllerWrapper);
};
}  // namespace network
}  // namespace webrtc

#endif  // NETWORK_CONTROL_NETWORK_CONTROLLER_H_
