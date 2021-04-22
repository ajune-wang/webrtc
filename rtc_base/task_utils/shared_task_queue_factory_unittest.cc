/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/shared_task_queue_factory.h"

#include <memory>

#include "api/priority.h"
#include "api/sequence_checker.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/task_queue/task_queue_test.h"
#include "rtc_base/event.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

struct SharedTaskQueueFactoryAssembly : public TaskQueueFactory {
  std::unique_ptr<TaskQueueFactory> base_factory =
      CreateDefaultTaskQueueFactory();
  std::unique_ptr<TaskQueueFactory> shared_factory =
      CreateSharedTaskQueueFactory(base_factory.get());

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override {
    return shared_factory->CreateTaskQueue(name, priority);
  }
};

using ::testing::Values;
using ::webrtc::TaskQueueTest;

TEST(SharedTaskQueueFactoryTest, RunsTasksFromDistinctFactoriesInIsolation) {
  bool in_sequence = false;

  struct ControlBlock {
    rtc::Event completion;
    std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue;
  } control_blocks[10];
  std::unique_ptr<TaskQueueFactory> base_factory =
      CreateDefaultTaskQueueFactory();
  std::unique_ptr<TaskQueueFactory> shared_factory =
      CreateSharedTaskQueueFactory(base_factory.get());
  for (auto& control_block : control_blocks) {
    control_block.task_queue = shared_factory->CreateTaskQueue(
        "RunsTasksFromDistinctFactoriesOnSameSequence",
        TaskQueueFactory::Priority::NORMAL);
  }
  for (auto& control_block : control_blocks) {
    control_block.task_queue->PostTask(
        ToQueuedTask([&control_block, &in_sequence] {
          EXPECT_FALSE(in_sequence);
          in_sequence = true;
          // Increase chance of showing any problem by waiting for a
          // millisecond. On Mac, using the |base_factory| instead only failed
          // the test 87% of the times for 100 task queues. With 1 ms wait we
          // fail 100% with 10 task queues.
          control_block.completion.Wait(1);
          control_block.completion.Set();
          EXPECT_TRUE(in_sequence);
          in_sequence = false;
        }));
  }
  for (auto& control_block : control_blocks) {
    control_block.completion.Wait(rtc::Event::kForever);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Shared,
    TaskQueueTest,
    Values([]() -> std::unique_ptr<TaskQueueFactory> {
      return std::make_unique<SharedTaskQueueFactoryAssembly>();
    }));

}  // namespace
}  // namespace webrtc
