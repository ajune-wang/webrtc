/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/task_queue/default_task_queue_factory.h"

#include "api/task_queue/task_queue_test.h"

namespace webrtc {
namespace {

std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateDefaultTaskQueue(
    absl::string_view name,
    TaskQueueFactory::Priority priority) {
  static auto factory = CreateDefaultTaskQueueFactory();
  return factory->CreateTaskQueue(name, priority);
}

INSTANTIATE_TEST_CASE_P(Default,
                        TaskQueueTest,
                        ::testing::Values(&CreateDefaultTaskQueue));

}  // namespace
}  // namespace webrtc
