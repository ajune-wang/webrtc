/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/active_ice_controller_adapter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/ice_transport_interface.h"
#include "api/sequence_checker.h"
#include "p2p/base/active_ice_controller_factory_interface.h"
#include "p2p/base/basic_ice_controller.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_controller_adapter_interface.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/base/wrapping_active_ice_controller.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/trace_event.h"

namespace cricket {

ActiveIceControllerAdapter::ActiveIceControllerAdapter(
    const IceControllerAdapterArgs& args)
    : network_thread_(rtc::Thread::Current()) {
  RTC_LOG(LS_INFO) << "Constructing an ActiveIceControllerAdapter";
  if (args.active_ice_controller_factory != nullptr) {
    ActiveIceControllerFactoryArgs active_args{args.ice_controller_factory_args,
                                               args.ice_agent, args.observer};
    active_ice_controller_ =
        args.active_ice_controller_factory->Create(active_args);
  } else {
    RTC_LOG(LS_WARNING)
        << "Constructing an ActiveIceControllerAdapter without an active ICE "
           "controller factory, defaulting to a wrapped Basic ICE controller.";
    std::unique_ptr<IceControllerInterface> wrapped;
    if (args.legacy_ice_controller_factory != nullptr) {
      wrapped = args.legacy_ice_controller_factory->Create(
          args.ice_controller_factory_args);
    } else {
      wrapped = std::make_unique<BasicIceController>(
          args.ice_controller_factory_args);
    }
    active_ice_controller_ = std::make_unique<WrappingActiveIceController>(
        args.ice_agent, args.observer, std::move(wrapped));
  }
  RTC_LOG(LS_INFO) << "Finished constructing an ActiveIceControllerAdapter";
}

void ActiveIceControllerAdapter::SetIceConfig(const IceConfig& config) {
  RTC_LOG(LS_INFO) << "ActiveIceControllerAdapter::SetIceControllerConfig";
  RTC_DCHECK_RUN_ON(network_thread_);
  active_ice_controller_->SetIceConfig(config);
}

bool ActiveIceControllerAdapter::GetUseCandidateAttr(
    Connection* conn,
    NominationMode nomination_mode,
    IceMode remote_ice_mode) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return active_ice_controller_->GetUseCandidateAttribute(conn, nomination_mode,
                                                          remote_ice_mode);
}

void ActiveIceControllerAdapter::AddConnection(Connection* connection) {
  RTC_LOG(LS_INFO) << "ActiveIceControllerAdapter::NotifyConnectionAdded";
  RTC_DCHECK_RUN_ON(network_thread_);
  // TODO(samvi) may need to generate candidate report in more situations.
  connection->SignalReadyToSend.connect(
      this, &ActiveIceControllerAdapter::MaybeGenerateConnectionReport);
  connection->SignalStateChange.connect(
      this, &ActiveIceControllerAdapter::MaybeGenerateConnectionReport);
  connection->SignalNominated.connect(
      this, &ActiveIceControllerAdapter::MaybeGenerateConnectionReport);
  active_ice_controller_->OnConnectionAdded(connection);
}

void ActiveIceControllerAdapter::MaybeGenerateConnectionReport(
    Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  // TODO(samvi) check if anything has actually changed.
  active_ice_controller_->OnConnectionReport(connection);
}

void ActiveIceControllerAdapter::SetSelectedConnection(
    const Connection* connection) {
  RTC_LOG(LS_INFO) << "ActiveIceControllerAdapter::SetSelectedConnection";
  RTC_DCHECK_RUN_ON(network_thread_);
  active_ice_controller_->OnConnectionSwitched(connection);
}

void ActiveIceControllerAdapter::OnConnectionDestroyed(
    const Connection* connection) {
  RTC_LOG(LS_INFO) << "ActiveIceControllerAdapter::NotifyConnectionDestroyed";
  RTC_DCHECK_RUN_ON(network_thread_);
  active_ice_controller_->OnConnectionDestroyed(connection);
}

void ActiveIceControllerAdapter::MaybeStartPinging() {
  RTC_LOG(LS_INFO) << "ActiveIceControllerAdapter::MaybeStartPinging";
  RTC_DCHECK_RUN_ON(network_thread_);
  active_ice_controller_->OnStartPingingRequest();
}

void ActiveIceControllerAdapter::RequestSortAndStateUpdate(
    IceSwitchReason reason_to_sort) {
  RTC_LOG(LS_INFO) << "ActiveIceControllerAdapter::RequestSortAndStateUpdate";
  RTC_DCHECK_RUN_ON(network_thread_);
  active_ice_controller_->OnSortAndSwitchRequest(reason_to_sort);
}

void ActiveIceControllerAdapter::SortConnectionsAndUpdateState(
    IceSwitchReason reason_to_sort) {
  RTC_LOG(LS_INFO)
      << "ActiveIceControllerAdapter::SortConnectionsAndUpdateState";
  RTC_DCHECK_RUN_ON(network_thread_);
  active_ice_controller_->OnImmediateSortAndSwitchRequest(reason_to_sort);
}

bool ActiveIceControllerAdapter::MaybeSwitchSelectedConnection(
    Connection* new_connection,
    IceSwitchReason reason) {
  RTC_LOG(LS_INFO)
      << "ActiveIceControllerAdapter::MaybeSwitchSelectedConnection";
  RTC_DCHECK_RUN_ON(network_thread_);
  return active_ice_controller_->OnImmediateSwitchRequest(reason,
                                                          new_connection);
}

// This method is only for unit testing.
const Connection* ActiveIceControllerAdapter::FindNextPingableConnection() {
  RTC_DCHECK_RUN_ON(network_thread_);
  return active_ice_controller_->FindNextPingableConnection();
}

// This method is only for unit testing.
void ActiveIceControllerAdapter::MarkConnectionPinged(Connection* conn) {
  RTC_DCHECK_RUN_ON(network_thread_);
  active_ice_controller_->OnConnectionPinged(conn);
}

// This method is only for unit testing.
rtc::ArrayView<const Connection*> ActiveIceControllerAdapter::Connections()
    const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return active_ice_controller_->Connections();
}

}  // namespace cricket
