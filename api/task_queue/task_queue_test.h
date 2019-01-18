/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TASK_QUEUE_TASK_QUEUE_TEST_H_
#define API_TASK_QUEUE_TASK_QUEUE_TEST_H_

#include <functional>
#include <memory>

#include "absl/strings/string_view.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "test/gtest.h"

namespace webrtc {

class TaskQueueTest
    : public ::testing::TestWithParam<
          std::function<std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
              absl::string_view name,
              TaskQueueFactory::Priority priority)>> {};

}  // namespace webrtc

#endif  // API_TASK_QUEUE_TASK_QUEUE_TEST_H_
