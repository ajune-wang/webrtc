/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ACTIVE_ICE_CONTROLLER_ADAPTER_H_
#define P2P_BASE_ACTIVE_ICE_CONTROLLER_ADAPTER_H_

#include <memory>

#include "api/array_view.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "p2p/base/active_ice_controller_interface.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_controller_adapter_interface.h"
#include "p2p/base/ice_switch_reason.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

namespace cricket {

// ActiveIceControllerAdapter wraps over active ICE controllers that actively
// keep state and invoke actions on the IceAgent when needed. This is mainly a
// translation between the IceControllerAdapterInterface and
// ActiveIceControllerInterface.
class ActiveIceControllerAdapter : public IceControllerAdapterInterface,
                                   public sigslot::has_slots<> {
 public:
  explicit ActiveIceControllerAdapter(const IceControllerAdapterArgs& args);
  ~ActiveIceControllerAdapter() override = default;

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

  // For unit tests only
  rtc::ArrayView<const Connection*> Connections() const override;
  const Connection* FindNextPingableConnection() override;
  void MarkConnectionPinged(Connection* conn) override;

 private:
  void MaybeGenerateConnectionReport(Connection* connection);

  rtc::Thread* const network_thread_;
  webrtc::ScopedTaskSafety task_safety_;
  std::unique_ptr<ActiveIceControllerInterface> active_ice_controller_
      RTC_GUARDED_BY(network_thread_);
};

}  // namespace cricket

#endif  // P2P_BASE_ACTIVE_ICE_CONTROLLER_ADAPTER_H_
