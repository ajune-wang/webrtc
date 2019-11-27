/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_MOCK_CONNECTION_ICE_CONTROLLER_INTERFACE_H_
#define P2P_BASE_MOCK_CONNECTION_ICE_CONTROLLER_INTERFACE_H_

#include "api/candidate.h"
#include "p2p/base/connection_ice_controller_interface.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace cricket {

// Used in Chromium/remoting/protocol/channel_socket_adapter_unittest.cc
class MockConnectionIceControllerInterface
    : public ConnectionIceControllerInterface {
 public:
  ~MockConnectionIceControllerInterface() = default;

  MOCK_CONST_METHOD0(local_candidate, const Candidate&());
  MOCK_CONST_METHOD0(remote_candidate, const Candidate&());
  MOCK_CONST_METHOD0(network, const rtc::Network*());
  MOCK_CONST_METHOD0(generation, int());
  MOCK_CONST_METHOD0(priority, uint64_t());
  MOCK_CONST_METHOD0(remote_nomination, uint32_t());
};

}  // namespace cricket

#endif  // P2P_BASE_MOCK_CONNECTION_ICE_CONTROLLER_INTERFACE_H_
