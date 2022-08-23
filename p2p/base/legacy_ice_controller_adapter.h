/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_LEGACY_ICE_CONTROLLER_ADAPTER_H_
#define P2P_BASE_LEGACY_ICE_CONTROLLER_ADAPTER_H_

#include <memory>

#include "api/array_view.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_agent_interface.h"
#include "p2p/base/ice_controller_adapter_interface.h"
#include "p2p/base/ice_controller_interface.h"
#include "p2p/base/ice_switch_reason.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

namespace cricket {

// LegacyIceControllerAdapter wraps over legacy ICE controllers that operate
// passively, i.e. only compute a result for a task requested by the
// IceTransport. The LegacyIceControllerAdapter forwards these tasks to the
// wrapped IceController, and in response to the results invokes methods on the
// IceAgent (often same as the IceTransport). This allows legacy ICE controllers
// to act like active ICE controllers without any changes.
class LegacyIceControllerAdapter : public IceControllerAdapterInterface {
 public:
  explicit LegacyIceControllerAdapter(const IceControllerAdapterArgs& args);
  ~LegacyIceControllerAdapter() override = default;

  void SetIceConfig(const IceConfig& config) override;
  bool GetUseCandidateAttr(Connection* conn,
                           NominationMode nomination_mode,
                           IceMode remote_ice_mode) const override;

  void AddConnection(Connection* connection) override;
  void SetSelectedConnection(const Connection* connection) override;
  void OnConnectionDestroyed(const Connection* connection) override;

  // Start pinging if we haven't already started, and we now have a connection
  // that's pingable.
  void MaybeStartPinging() override;

  void RequestSortAndStateUpdate(IceSwitchReason reason_to_sort) override;
  void SortConnectionsAndUpdateState(IceSwitchReason reason_to_sort) override;
  bool MaybeSwitchSelectedConnection(Connection* new_connection,
                                     IceSwitchReason reason) override;

  void ProcessPingRequest(const PingRequest& unused) override {
    // This action is only available with active controllers, and should never
    // be invoked for a legacy ICE controller.
    RTC_DCHECK_NOTREACHED();
  }
  void ProcessSwitchRequest(const SwitchRequest& unused) override {
    // This action is only available with active controllers, and should never
    // be invoked for a legacy ICE controller.
    RTC_DCHECK_NOTREACHED();
  }

  // For unit tests only
  rtc::ArrayView<const Connection*> Connections() const override;
  const Connection* FindNextPingableConnection() override;
  void MarkConnectionPinged(Connection* conn) override;

 private:
  // Returns true if the new_connection is selected for transmission.
  void MaybeSwitchSelectedConnection(
      IceSwitchReason reason,
      IceControllerInterface::SwitchResult result);

  void CheckAndPing();
  void PruneConnections();

  rtc::Thread* const network_thread_;
  webrtc::ScopedTaskSafety task_safety_;
  bool sort_dirty_ RTC_GUARDED_BY(
      network_thread_);  // indicates whether another sort is needed right now
  bool started_pinging_ RTC_GUARDED_BY(network_thread_) = false;
  std::unique_ptr<IceControllerInterface> ice_controller_
      RTC_GUARDED_BY(network_thread_);
  IceAgentInterface& agent_ RTC_GUARDED_BY(network_thread_);
};

}  // namespace cricket

#endif  // P2P_BASE_LEGACY_ICE_CONTROLLER_ADAPTER_H_
