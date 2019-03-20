
/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_CREATE_NETWORK_EMULATION_MANAGER_H_
#define API_TEST_CREATE_NETWORK_EMULATION_MANAGER_H_

#include <memory>

#include "api/task_queue/task_queue_factory.h"
#include "api/test/network_emulation_manager.h"

namespace webrtc {

std::unique_ptr<NetworkEmulationManager> CreateNetworkEmulationManager(
    TaskQueueFactory* task_queue_factory);

}  // namespace webrtc

#endif  // API_TEST_CREATE_NETWORK_EMULATION_MANAGER_H_
