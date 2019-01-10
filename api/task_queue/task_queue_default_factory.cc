/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/task_queue/task_queue_default_factory.h"

namespace webrtc {

TaskQueueFactory::TaskQueuePtr DefaultTaskQueueFactory::CreateTaskQueue(
    const char* name,
    Priority priority) const {
  // TODO(danilchap): Create matching implementaion depending on the platform.
  // return MakeTaskQueuePtr<rtc::TaskQueue::Impl>(name, priority);
  return nullptr;
}

}  // namespace webrtc
