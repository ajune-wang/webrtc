/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/legacy_ice_controller_adapter.h"

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/units/time_delta.h"
#include "p2p/base/basic_ice_controller.h"
#include "p2p/base/ice_controller_adapter_interface.h"
#include "p2p/base/ice_controller_interface.h"
#include "p2p/base/ice_switch_reason.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/thread.h"

namespace {
using ::webrtc::SafeTask;
using ::webrtc::TimeDelta;
}  // unnamed namespace

namespace cricket {

LegacyIceControllerAdapter::LegacyIceControllerAdapter(
    const IceControllerAdapterArgs& args)
    : network_thread_(rtc::Thread::Current()),
      sort_dirty_(false),
      agent_(*args.ice_agent) {
  RTC_DCHECK(args.ice_agent != nullptr);
  if (args.legacy_ice_controller_factory != nullptr) {
    ice_controller_ = args.legacy_ice_controller_factory->Create(
        args.ice_controller_factory_args);
  } else {
    ice_controller_ =
        std::make_unique<BasicIceController>(args.ice_controller_factory_args);
  }
}

void LegacyIceControllerAdapter::SetIceConfig(const IceConfig& config) {
  RTC_DCHECK_RUN_ON(network_thread_);
  ice_controller_->SetIceConfig(config);
}

// Nominate a connection based on the NominationMode.
bool LegacyIceControllerAdapter::GetUseCandidateAttr(
    Connection* conn,
    NominationMode nomination_mode,
    IceMode remote_ice_mode) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return ice_controller_->GetUseCandidateAttr(conn, nomination_mode,
                                              remote_ice_mode);
}

void LegacyIceControllerAdapter::AddConnection(Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  ice_controller_->AddConnection(connection);
}

void LegacyIceControllerAdapter::SetSelectedConnection(
    const Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  ice_controller_->SetSelectedConnection(connection);
}

void LegacyIceControllerAdapter::OnConnectionDestroyed(
    const Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  ice_controller_->OnConnectionDestroyed(connection);
}

void LegacyIceControllerAdapter::MaybeStartPinging() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (started_pinging_) {
    return;
  }

  if (ice_controller_->HasPingableConnection()) {
    network_thread_->PostTask(
        SafeTask(task_safety_.flag(), [this]() { CheckAndPing(); }));
    agent_.OnStartedPinging();
    started_pinging_ = true;
  }
}

// Prepare for best candidate sorting.
void LegacyIceControllerAdapter::RequestSortAndStateUpdate(
    IceSwitchReason reason_to_sort) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!sort_dirty_) {
    network_thread_->PostTask(
        SafeTask(task_safety_.flag(), [this, reason_to_sort]() {
          SortConnectionsAndUpdateState(reason_to_sort);
        }));
    sort_dirty_ = true;
  }
}

// Sort the available connections to find the best one.  We also monitor
// the number of available connections and the current state.
void LegacyIceControllerAdapter::SortConnectionsAndUpdateState(
    IceSwitchReason reason_to_sort) {
  RTC_DCHECK_RUN_ON(network_thread_);

  // Make sure the connection states are up-to-date since this affects how they
  // will be sorted.
  agent_.UpdateConnectionStates();

  // Any changes after this point will require a re-sort.
  sort_dirty_ = false;

  // If necessary, switch to the new choice. Note that `top_connection` doesn't
  // have to be writable to become the selected connection although it will
  // have higher priority if it is writable.
  IceControllerInterface::SwitchResult result =
      ice_controller_->SortAndSwitchConnection(reason_to_sort);
  MaybeSwitchSelectedConnection(reason_to_sort, result);

  if (agent_.ShouldPruneConnections()) {
    PruneConnections();
  }

  agent_.OnConnectionsResorted();
}

bool LegacyIceControllerAdapter::MaybeSwitchSelectedConnection(
    Connection* new_connection,
    IceSwitchReason reason) {
  RTC_DCHECK_RUN_ON(network_thread_);

  IceControllerInterface::SwitchResult result =
      ice_controller_->ShouldSwitchConnection(reason, new_connection);
  MaybeSwitchSelectedConnection(reason, result);
  return result.connection.has_value();
}

void LegacyIceControllerAdapter::MaybeSwitchSelectedConnection(
    IceSwitchReason reason,
    IceControllerInterface::SwitchResult result) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (result.connection.has_value()) {
    RTC_LOG(LS_INFO) << "Switching selected connection due to: "
                     << IceSwitchReasonToString(reason);
    agent_.SwitchSelectedConnection(*result.connection, reason);
  }

  if (result.recheck_event.has_value()) {
    // If we do not switch to the connection because it missed the receiving
    // threshold, the new connection is in a better receiving state than the
    // currently selected connection. So we need to re-check whether it needs
    // to be switched at a later time.
    network_thread_->PostDelayedTask(
        SafeTask(task_safety_.flag(),
                 [this, reason = result.recheck_event->reason]() {
                   SortConnectionsAndUpdateState(reason);
                 }),
        TimeDelta::Millis(result.recheck_event->recheck_delay_ms));
  }

  agent_.ForgetLearnedStateForConnections(
      result.connections_to_forget_state_on);
}

// Handle queued up check-and-ping request
void LegacyIceControllerAdapter::CheckAndPing() {
  RTC_DCHECK_RUN_ON(network_thread_);
  // Make sure the states of the connections are up-to-date (since this affects
  // which ones are pingable).
  agent_.UpdateConnectionStates();

  auto result =
      ice_controller_->SelectConnectionToPing(agent_.GetLastPingSentMs());

  if (result.connection.value_or(nullptr)) {
    agent_.SendPingRequest(*result.connection);
  }

  network_thread_->PostDelayedTask(
      SafeTask(task_safety_.flag(), [this]() { CheckAndPing(); }),
      TimeDelta::Millis(result.recheck_delay_ms));
}

void LegacyIceControllerAdapter::PruneConnections() {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<const Connection*> connections_to_prune =
      ice_controller_->PruneConnections();
  agent_.PruneConnections(connections_to_prune);
}

// This method is only for unit testing.
const Connection* LegacyIceControllerAdapter::FindNextPingableConnection() {
  RTC_DCHECK_RUN_ON(network_thread_);
  return ice_controller_->FindNextPingableConnection();
}

// A connection is considered a backup connection if the channel state
// is completed, the connection is not the selected connection and it is active.
void LegacyIceControllerAdapter::MarkConnectionPinged(Connection* conn) {
  RTC_DCHECK_RUN_ON(network_thread_);
  ice_controller_->MarkConnectionPinged(conn);
}

// This method is only for unit testing.
rtc::ArrayView<const Connection*> LegacyIceControllerAdapter::Connections()
    const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return ice_controller_->connections();
}

}  // namespace cricket
