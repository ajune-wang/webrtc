/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SDP_STATE_INFO_H_
#define PC_SDP_STATE_INFO_H_

#include <string>

#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "pc/jsep_transport_controller.h"

namespace webrtc {

// This interface offers access to the state of an SDP offer/answer
// negotiation.
//
// All the functions are const, so using this interface serves as
// assurance that the user is not modifying the state.
class SdpStateInfoInterface {
 public:
  virtual ~SdpStateInfoInterface() {}

  virtual PeerConnectionInterface::SignalingState signaling_state() const = 0;

  virtual const SessionDescriptionInterface* local_description() const = 0;
  virtual const SessionDescriptionInterface* remote_description() const = 0;
  virtual const SessionDescriptionInterface* current_local_description()
      const = 0;
  virtual const SessionDescriptionInterface* current_remote_description()
      const = 0;
  virtual const SessionDescriptionInterface* pending_local_description()
      const = 0;
  virtual const SessionDescriptionInterface* pending_remote_description()
      const = 0;

  virtual const JsepTransportController* transport_controller() const = 0;
  virtual bool IceRestartPending(const std::string& content_name) const = 0;
};

}  // namespace webrtc

#endif  // PC_SDP_STATE_INFO_H_
