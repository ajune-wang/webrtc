/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOCC_INCLUDE_GOCC_FACTORY_H_
#define MODULES_CONGESTION_CONTROLLER_GOCC_INCLUDE_GOCC_FACTORY_H_
#include "network_control/include/network_control.h"

namespace webrtc {
class Clock;
class RtcEventLog;

namespace network {
class GoccNetworkControllerFactory : public NetworkControllerFactoryInterface {
 public:
  GoccNetworkControllerFactory(const Clock*, RtcEventLog*);
  NetworkControllerInterface::uptr Create() override;

 private:
  const Clock* clock_;
  RtcEventLog* event_log_;
};
}  // namespace network
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOCC_INCLUDE_GOCC_FACTORY_H_
