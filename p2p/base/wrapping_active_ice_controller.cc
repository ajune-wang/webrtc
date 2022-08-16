/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/wrapping_active_ice_controller.h"

#include <memory>
#include <utility>

#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/units/time_delta.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_agent_interface.h"
#include "p2p/base/ice_controller_interface.h"
#include "p2p/base/ice_switch_reason.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"

namespace {
using ::webrtc::SafeTask;
using ::webrtc::TimeDelta;
}  // unnamed namespace

namespace cricket {

WrappingActiveIceController::WrappingActiveIceController(
    IceAgentInterface* ice_agent,
    std::unique_ptr<IceControllerInterface> wrapped)
    : network_thread_(rtc::Thread::Current()),
      wrapped_(std::move(wrapped)),
      agent_(*ice_agent) {
  RTC_DCHECK(ice_agent != nullptr);
}

WrappingActiveIceController::~WrappingActiveIceController() {}

void WrappingActiveIceController::SetIceConfig(const IceConfig& config) {
  RTC_DCHECK_RUN_ON(network_thread_);
  wrapped_->SetIceConfig(config);
}

bool WrappingActiveIceController::GetUseCandidateAttribute(
    const Connection* connection,
    NominationMode mode,
    IceMode remote_ice_mode) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return wrapped_->GetUseCandidateAttr(connection, mode, remote_ice_mode);
}

rtc::ArrayView<const Connection*> WrappingActiveIceController::Connections()
    const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return wrapped_->connections();
}

void WrappingActiveIceController::OnConnectionAdded(
    const Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  wrapped_->AddConnection(connection);
}

void WrappingActiveIceController::OnConnectionPinged(
    const Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  wrapped_->MarkConnectionPinged(connection);
}

void WrappingActiveIceController::OnConnectionReport(
    const Connection* connection) {
  RTC_LOG(LS_VERBOSE) << "Connection report for " << connection->ToString();
  // Do nothing. Native ICE controllers have direct access to Connection, so no
  // need to update connection state separately.
}

void WrappingActiveIceController::OnConnectionSwitched(
    const Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  wrapped_->SetSelectedConnection(connection);
}

void WrappingActiveIceController::OnConnectionDestroyed(
    const Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  wrapped_->OnConnectionDestroyed(connection);
}

void WrappingActiveIceController::OnStartPingingRequest() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (started_pinging_) {
    return;
  }

  if (wrapped_->HasPingableConnection()) {
    network_thread_->PostTask(
        SafeTask(task_safety_.flag(), [this]() { PingBestConnection(); }));
    started_pinging_ = true;
    agent_.OnStartedPinging();
  }
}

void WrappingActiveIceController::OnSortAndSwitchRequest(
    IceSwitchReason reason) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!sort_pending_) {
    network_thread_->PostTask(SafeTask(task_safety_.flag(), [this, reason]() {
      SwitchToBestConnectionAndPrune(reason);
    }));
    sort_pending_ = true;
  }
}

void WrappingActiveIceController::OnImmediateSortAndSwitchRequest(
    IceSwitchReason reason) {
  RTC_DCHECK_RUN_ON(network_thread_);
  SwitchToBestConnectionAndPrune(reason);
}

bool WrappingActiveIceController::OnImmediateSwitchRequest(
    IceSwitchReason reason,
    const Connection* selected) {
  RTC_DCHECK_RUN_ON(network_thread_);
  IceControllerInterface::SwitchResult result =
      wrapped_->ShouldSwitchConnection(reason, selected);
  HandleSwitchResult(reason, result);
  return result.connection.has_value();
}

// Only for unit tests
const Connection* WrappingActiveIceController::FindNextPingableConnection() {
  RTC_DCHECK_RUN_ON(network_thread_);
  return wrapped_->FindNextPingableConnection();
}

void WrappingActiveIceController::PingBestConnection() {
  RTC_DCHECK_RUN_ON(network_thread_);
  agent_.UpdateConnectionStates();

  IceControllerInterface::PingResult result =
      wrapped_->SelectConnectionToPing(agent_.GetLastPingSentMs());
  HandlePingResult(result);
}

void WrappingActiveIceController::HandlePingResult(
    IceControllerInterface::PingResult result) {
  RTC_DCHECK_RUN_ON(network_thread_);

  if (result.connection.value_or(nullptr)) {
    agent_.SendPingRequest(*result.connection);
  }

  network_thread_->PostDelayedTask(
      SafeTask(task_safety_.flag(), [this]() { PingBestConnection(); }),
      TimeDelta::Millis(result.recheck_delay_ms));
}

void WrappingActiveIceController::SwitchToBestConnectionAndPrune(
    IceSwitchReason reason) {
  RTC_DCHECK_RUN_ON(network_thread_);
  agent_.UpdateConnectionStates();
  sort_pending_ = false;

  IceControllerInterface::SwitchResult result =
      wrapped_->SortAndSwitchConnection(reason);
  HandleSwitchResult(reason, result);

  if (agent_.ShouldPruneConnections()) {
    agent_.PruneConnections(wrapped_->PruneConnections());
  }

  agent_.OnConnectionsResorted();
}

void WrappingActiveIceController::HandleSwitchResult(
    IceSwitchReason reason_for_switch,
    IceControllerInterface::SwitchResult result) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (result.connection.has_value()) {
    agent_.SwitchSelectedConnection(*(result.connection), reason_for_switch);
  }
  if (result.recheck_event.has_value()) {
    network_thread_->PostDelayedTask(
        SafeTask(task_safety_.flag(),
                 [this, recheck_reason = result.recheck_event->reason]() {
                   SwitchToBestConnectionAndPrune(recheck_reason);
                 }),
        TimeDelta::Millis(result.recheck_event->recheck_delay_ms));
  }
  agent_.ForgetLearnedStateForConnections(
      result.connections_to_forget_state_on);
}

}  // namespace cricket
