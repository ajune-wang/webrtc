/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROLLERS_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROLLERS_H_
#include <memory>
#include "api/optional.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "network_control/include/network_control.h"
#include "rtc_base/task_queue.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace network {
std::unique_ptr<NetworkControllerInterface>
CreateDelayBasedNetworkController(const Clock*, RtcEventLog*, rtc::TaskQueue*);

std::unique_ptr<NetworkControllerInterface> CreateDelayBasedNetworkController(
    const Clock*,
    RtcEventLog*);

}  // namespace network
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROLLERS_H_
