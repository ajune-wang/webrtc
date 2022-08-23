/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICE_CONTROLLER_ADAPTER_INTERFACE_H_
#define P2P_BASE_ICE_CONTROLLER_ADAPTER_INTERFACE_H_

#include "p2p/base/active_ice_controller_factory_interface.h"
#include "p2p/base/active_ice_controller_interface.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_agent_interface.h"
#include "p2p/base/ice_controller_factory_interface.h"
#include "p2p/base/ice_controller_observer.h"
#include "p2p/base/ice_controller_request_types.h"
#include "p2p/base/ice_switch_reason.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/transport_description.h"

namespace cricket {

struct IceControllerAdapterArgs {
  IceControllerFactoryArgs ice_controller_factory_args;
  IceAgentInterface* ice_agent;
  IceControllerFactoryInterface* legacy_ice_controller_factory;
  ActiveIceControllerFactoryInterface* active_ice_controller_factory;
  IceControllerObserver* observer;
};

// An IceControllerAdapter interacts with an IceController on behalf of the
// IceTransport. This allows the IceController to be switched freely between
// legacy and active ICE controllers.
class IceControllerAdapterInterface {
 public:
  virtual ~IceControllerAdapterInterface() = default;

  virtual void SetIceConfig(const IceConfig& config) = 0;
  virtual bool GetUseCandidateAttr(Connection* conn,
                                   NominationMode nomination_mode,
                                   IceMode remote_ice_mode) const = 0;

  virtual void AddConnection(Connection* connection) = 0;
  virtual void SetSelectedConnection(const Connection* connection) = 0;
  virtual void OnConnectionDestroyed(const Connection* connection) = 0;

  // Start pinging if we haven't already started, and we now have a connection
  // that's pingable.
  virtual void MaybeStartPinging() = 0;

  virtual void RequestSortAndStateUpdate(IceSwitchReason reason_to_sort) = 0;
  virtual void SortConnectionsAndUpdateState(
      IceSwitchReason reason_to_sort) = 0;
  virtual bool MaybeSwitchSelectedConnection(Connection* new_connection,
                                             IceSwitchReason reason) = 0;

  virtual void ProcessPingRequest(const PingRequest& ping_request) = 0;
  virtual void ProcessSwitchRequest(const SwitchRequest& switch_request) = 0;

  // For unit tests only
  virtual rtc::ArrayView<const Connection*> Connections() const = 0;
  virtual const Connection* FindNextPingableConnection() = 0;
  virtual void MarkConnectionPinged(Connection* conn) = 0;
};

}  // namespace cricket

#endif  // P2P_BASE_ICE_CONTROLLER_ADAPTER_INTERFACE_H_
