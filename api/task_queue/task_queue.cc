/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/task_queue/task_queue.h"

#include "absl/base/attributes.h"

namespace webrtc {
namespace {

ABSL_CONST_INIT thread_local TaskQueue* current = nullptr;

}  // namespace

void TaskQueue::Delete() {
  delete this;
}

TaskQueue* TaskQueue::Current() {
  return current;
}

TaskQueue::CurrentTaskQueueSetter::CurrentTaskQueueSetter(TaskQueue* task_queue)
    : previous_(current) {
  current = task_queue;
}

TaskQueue::CurrentTaskQueueSetter::~CurrentTaskQueueSetter() {
  current = previous_;
}

}  // namespace webrtc
