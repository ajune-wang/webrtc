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

NetworkControlJunctions::~NetworkControlJunctions() {
  CongestionWindowJunction.Disconnect();
  PacerConfigJunction.Disconnect();
  ProbeClusterConfigJunction.Disconnect();
  TargetTransferRateJunction.Disconnect();
}

NetworkControlObservers::uptr NetworkControlJunctions::GetObservers() {
  NetworkControlObservers::uptr observers(
      rtc::MakeUnique<NetworkControlObservers>());
  observers->CongestionWindowObserver = &CongestionWindowJunction;
  observers->PacerConfigObserver = &PacerConfigJunction;
  observers->ProbeClusterConfigObserver = &ProbeClusterConfigJunction;
  observers->TargetTransferRateObserver = &TargetTransferRateJunction;
  return observers;
}

NetworkControlProducers::uptr NetworkControlJunctions::GetProducers() {
  NetworkControlProducers::uptr producers(
      rtc::MakeUnique<NetworkControlProducers>());
  producers->CongestionWindowProducer = &CongestionWindowJunction;
  producers->PacerConfigProducer = &PacerConfigJunction;
  producers->ProbeClusterConfigProducer = &ProbeClusterConfigJunction;
  producers->TargetTransferRateProducer = &TargetTransferRateJunction;
  return producers;
}

NetworkControlHandlingReceivers::NetworkControlHandlingReceivers(
    rtc::TaskQueue* task_queue,
    NetworkControllerInternalInterface* controller) {
  NetworkAvailabilityReceiver =
      NetworkAvailability::CreateReceiver(task_queue, &lock_);
  NetworkRouteChangeReceiver =
      NetworkRouteChange::CreateReceiver(task_queue, &lock_);
  ProcessIntervalReceiver = ProcessInterval::CreateReceiver(task_queue, &lock_);
  RemoteBitrateReportReceiver =
      RemoteBitrateReport::CreateReceiver(task_queue, &lock_);
  RoundTripTimeReportReceiver =
      RoundTripTimeReport::CreateReceiver(task_queue, &lock_);
  SentPacketReceiver = SentPacket::CreateReceiver(task_queue, &lock_);
  StreamsConfigReceiver = StreamsConfig::CreateReceiver(task_queue, &lock_);
  TransferRateConstraintsReceiver =
      TargetRateConstraints::CreateReceiver(task_queue, &lock_);
  TransportLossReportReceiver =
      TransportLossReport::CreateReceiver(task_queue, &lock_);
  TransportPacketsFeedbackReceiver =
      TransportPacketsFeedback::CreateReceiver(task_queue, &lock_);
  controller->ConnectHandlers(GetHandlers());
}

NetworkControlHandlingReceivers::NetworkControlHandlingReceivers(
    NetworkControllerInternalInterface* controller)
    : NetworkControlHandlingReceivers(nullptr, controller) {}

NetworkControlHandlingReceivers::~NetworkControlHandlingReceivers() {}

NetworkControlReceivers::uptr NetworkControlHandlingReceivers::GetReceivers() {
  NetworkControlReceivers::uptr res(rtc::MakeUnique<NetworkControlReceivers>());
  res->NetworkAvailabilityReceiver = NetworkAvailabilityReceiver.get();
  res->NetworkRouteChangeReceiver = NetworkRouteChangeReceiver.get();
  res->ProcessIntervalReceiver = ProcessIntervalReceiver.get();
  res->RemoteBitrateReportReceiver = RemoteBitrateReportReceiver.get();
  res->RoundTripTimeReportReceiver = RoundTripTimeReportReceiver.get();
  res->SentPacketReceiver = SentPacketReceiver.get();
  res->StreamsConfigReceiver = StreamsConfigReceiver.get();
  res->TransferRateConstraintsReceiver = TransferRateConstraintsReceiver.get();
  res->TransportLossReportReceiver = TransportLossReportReceiver.get();
  res->TransportPacketsFeedbackReceiver =
      TransportPacketsFeedbackReceiver.get();
  return res;
}

NetworkControlHandlers::uptr NetworkControlHandlingReceivers::GetHandlers() {
  NetworkControlHandlers::uptr handlers(
      rtc::MakeUnique<NetworkControlHandlers>());
  handlers->NetworkAvailabilityHandler = NetworkAvailabilityReceiver.get();
  handlers->NetworkRouteChangeHandler = NetworkRouteChangeReceiver.get();
  handlers->ProcessIntervalHandler = ProcessIntervalReceiver.get();
  handlers->RemoteBitrateReportHandler = RemoteBitrateReportReceiver.get();
  handlers->RoundTripTimeReportHandler = RoundTripTimeReportReceiver.get();
  handlers->SentPacketHandler = SentPacketReceiver.get();
  handlers->StreamsConfigHandler = StreamsConfigReceiver.get();
  handlers->TransferRateConstraintsHandler =
      TransferRateConstraintsReceiver.get();
  handlers->TransportLossReportHandler = TransportLossReportReceiver.get();
  handlers->TransportPacketsFeedbackHandler =
      TransportPacketsFeedbackReceiver.get();
  return handlers;
}

NetworkControllerWrapper::NetworkControllerWrapper(
    NetworkControlJunctions::uptr junctions,
    NetworkControllerInternalInterface::uptr controller,
    NetworkControlHandlingReceivers::uptr receivers)
    : junctions_(std::move(junctions)),
      controller_(std::move(controller)),
      receivers_(std::move(receivers)) {}

NetworkControllerWrapper::~NetworkControllerWrapper() {}

NetworkControlProducers::uptr NetworkControllerWrapper::GetProducers() {
  return junctions_->GetProducers();
}

NetworkControlReceivers::uptr NetworkControllerWrapper::GetReceivers() {
  return receivers_->GetReceivers();
}

units::TimeDelta NetworkControllerWrapper::GetProcessInterval() {
  return controller_->GetProcessInterval();
}
}  // namespace network
}  // namespace webrtc
