/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "network_control/network_controller.h"

#include <algorithm>
#include <functional>
#include <utility>

namespace webrtc {
namespace network {
NetworkControlJunctions::NetworkControlJunctions() {}

NetworkControlJunctions::~NetworkControlJunctions() {}

NetworkControlProducers::uptr NetworkControlJunctions::GetProducers() {
  NetworkControlProducers::uptr producers(
      rtc::MakeUnique<NetworkControlProducers>());
  // producers->NetworkEstimateProducer = &NetworkEstimateJunction;
  producers->TargetTransferRateProducer = &TargetTransferRateJunction;
  producers->PacerConfigProducer = &PacerConfigJunction;
  producers->CongestionWindowProducer = &CongestionWindowJunction;
  producers->ProbeClusterConfigProducer = &ProbeClusterConfigJunction;
  return producers;
}

NetworkControlHandlingReceivers::NetworkControlHandlingReceivers() {}
NetworkControlHandlingReceivers::~NetworkControlHandlingReceivers() {}

NetworkControlReceivers::uptr NetworkControlHandlingReceivers::GetReceivers() {
  NetworkControlReceivers::uptr res(rtc::MakeUnique<NetworkControlReceivers>());
  res->TransportLossReportReceiver = TransportLossReportReceiver.get();
  res->RoundTripTimeReportReceiver = RoundTripTimeReportReceiver.get();
  res->RemoteBitrateReportReceiver = RemoteBitrateReportReceiver.get();
  res->TransportPacketsFeedbackReceiver =
      TransportPacketsFeedbackReceiver.get();
  res->NetworkRouteChangeReceiver = NetworkRouteChangeReceiver.get();

  res->SentPacketReceiver = SentPacketReceiver.get();
  res->NetworkAvailabilityReceiver = NetworkAvailabilityReceiver.get();
  res->TransferRateConstraintsReceiver = TransferRateConstraintsReceiver.get();
  res->StreamsConfigReceiver = StreamsConfigReceiver.get();

  res->ProcessIntervalReceiver = ProcessIntervalReceiver.get();
  return res;
}

NetworkInformationHandlers::uptr
NetworkControlHandlingReceivers::GetHandlers() {
  NetworkInformationHandlers::uptr handlers(
      rtc::MakeUnique<NetworkInformationHandlers>());
  handlers->SentPacketHandler = SentPacketReceiver.get();
  handlers->TransportPacketsFeedbackHandler =
      TransportPacketsFeedbackReceiver.get();
  handlers->TransportLossReportHandler = TransportLossReportReceiver.get();
  handlers->RoundTripTimeReportHandler = RoundTripTimeReportReceiver.get();
  handlers->RemoteBitrateReportHandler = RemoteBitrateReportReceiver.get();
  handlers->TransferRateConstraintsHandler =
      TransferRateConstraintsReceiver.get();
  handlers->StreamsConfigHandler = StreamsConfigReceiver.get();
  handlers->NetworkAvailabilityHandler = NetworkAvailabilityReceiver.get();
  handlers->NetworkRouteChangeHandler = NetworkRouteChangeReceiver.get();
  handlers->ProcessIntervalHandler = ProcessIntervalReceiver.get();
  return handlers;
}

TaskQueueNetworkControlReceivers::TaskQueueNetworkControlReceivers(
    rtc::TaskQueue* task_queue,
    NetworkControllerInternalInterface* controller) {
  SentPacketReceiver =
      rtc::MakeUnique<SentPacket::TaskQueueReceiver>(task_queue);
  TransportPacketsFeedbackReceiver =
      rtc::MakeUnique<TransportPacketsFeedback::TaskQueueReceiver>(task_queue);
  TransportLossReportReceiver =
      rtc::MakeUnique<TransportLossReport::TaskQueueReceiver>(task_queue);
  RoundTripTimeReportReceiver =
      rtc::MakeUnique<RoundTripTimeReport::TaskQueueReceiver>(task_queue);
  RemoteBitrateReportReceiver =
      rtc::MakeUnique<RemoteBitrateReport::TaskQueueReceiver>(task_queue);
  TransferRateConstraintsReceiver =
      rtc::MakeUnique<TargetRateConstraints::TaskQueueReceiver>(task_queue);
  StreamsConfigReceiver =
      rtc::MakeUnique<StreamsConfig::TaskQueueReceiver>(task_queue);
  NetworkAvailabilityReceiver =
      rtc::MakeUnique<NetworkAvailability::TaskQueueReceiver>(task_queue);
  NetworkRouteChangeReceiver =
      rtc::MakeUnique<NetworkRouteChange::TaskQueueReceiver>(task_queue);
  ProcessIntervalReceiver =
      rtc::MakeUnique<ProcessInterval::TaskQueueReceiver>(task_queue);

  controller->ConnectHandlers(GetHandlers());
}
TaskQueueNetworkControlReceivers::~TaskQueueNetworkControlReceivers() {}

LockedNetworkControlReceivers::LockedNetworkControlReceivers(
    NetworkControllerInternalInterface* controller) {
  SentPacketReceiver = rtc::MakeUnique<SentPacket::LockedReceiver>(&lock_);
  TransportPacketsFeedbackReceiver =
      rtc::MakeUnique<TransportPacketsFeedback::LockedReceiver>(&lock_);
  TransportLossReportReceiver =
      rtc::MakeUnique<TransportLossReport::LockedReceiver>(&lock_);
  RoundTripTimeReportReceiver =
      rtc::MakeUnique<RoundTripTimeReport::LockedReceiver>(&lock_);
  RemoteBitrateReportReceiver =
      rtc::MakeUnique<RemoteBitrateReport::LockedReceiver>(&lock_);
  TransferRateConstraintsReceiver =
      rtc::MakeUnique<TargetRateConstraints::LockedReceiver>(&lock_);
  StreamsConfigReceiver =
      rtc::MakeUnique<StreamsConfig::LockedReceiver>(&lock_);
  NetworkAvailabilityReceiver =
      rtc::MakeUnique<NetworkAvailability::LockedReceiver>(&lock_);
  NetworkRouteChangeReceiver =
      rtc::MakeUnique<NetworkRouteChange::LockedReceiver>(&lock_);
  ProcessIntervalReceiver =
      rtc::MakeUnique<ProcessInterval::LockedReceiver>(&lock_);
  controller->ConnectHandlers(GetHandlers());
}

LockedNetworkControlReceivers::~LockedNetworkControlReceivers() {}

NetworkControllerWrapper::NetworkControllerWrapper(
    NetworkControllerInternalInterface::uptr controller,
    NetworkControlHandlingReceivers::uptr receivers)
    : controller_(std::move(controller)), receivers_(std::move(receivers)) {}

NetworkControllerWrapper::~NetworkControllerWrapper() {}

NetworkControlReceivers::uptr NetworkControllerWrapper::GetReceivers() {
  return receivers_->GetReceivers();
}

NetworkControlProducers::uptr NetworkControllerWrapper::GetProducers() {
  return controller_->GetProducers();
}

units::TimeDelta NetworkControllerWrapper::GetProcessInterval() {
  return controller_->GetProcessInterval();
}
}  // namespace network
}  // namespace webrtc
