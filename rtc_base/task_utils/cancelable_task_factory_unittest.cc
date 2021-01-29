/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/cancelable_task_factory.h"

#include <memory>

#include "rtc_base/event.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int kAlmostForeverMs = 1000;

TEST(CancelableTaskFactoryTest, CanRunTaskBeforeCanceled) {
  CancelableTaskFactory factory;
  bool run = false;
  std::unique_ptr<QueuedTask> task = factory.CreateTask([&run] { run = true; });

  task->Run();

  EXPECT_TRUE(run);

  // Cleanup
  factory.CancelAll();
}

TEST(CancelableTaskFactoryTest, DoesntRunTaskAfterCanceled) {
  CancelableTaskFactory factory;
  bool run = false;
  std::unique_ptr<QueuedTask> task = factory.CreateTask([&run] { run = true; });

  factory.CancelAll();
  task->Run();

  EXPECT_FALSE(run);
}

TEST(CancelableTaskFactoryTest, CreatedTasksReturnsTrueWhenRun) {
  // CancelableTaskFactory creates only tasks that are designed to run once,
  // i.e. when posted to a TaskQueue, it would destroy them when they run.
  CancelableTaskFactory factory;
  std::unique_ptr<QueuedTask> task1 = factory.CreateTask([] {});
  std::unique_ptr<QueuedTask> task2 = factory.CreateTask([] {});

  EXPECT_TRUE(task1->Run());

  factory.CancelAll();

  // Returns true both before and after it was canceled.
  EXPECT_TRUE(task2->Run());
}

TEST(CancelableTaskFactoryTest, DoesntRunTaskCreatedAfterCanceled) {
  CancelableTaskFactory factory;
  factory.CancelAll();
  bool run = false;
  std::unique_ptr<QueuedTask> task = factory.CreateTask([&run] { run = true; });

  task->Run();

  EXPECT_FALSE(run);
}

TEST(CancelableTaskFactoryTest, StartedTaskBlocksCancelAll) {
  CancelableTaskFactory factory;
  rtc::Event task_started;
  rtc::Event unblock_task;
  std::unique_ptr<QueuedTask> task = factory.CreateTask([&] {
    task_started.Set();
    unblock_task.Wait(kAlmostForeverMs);
  });

  TaskQueueForTest thread1;
  thread1.PostTask(std::move(task));
  // Wait until task started before trying to cancel it.
  task_started.Wait(kAlmostForeverMs);

  // Cancel on a dedicated thread to avoid blocking main thread.
  TaskQueueForTest thread2;
  rtc::Event canceled;
  thread2.PostTask(ToQueuedTask([&] {
    factory.CancelAll();
    canceled.Set();
  }));

  // Expect thread2 is blocked.
  EXPECT_FALSE(canceled.Wait(/*ms=*/50));

  // Unblock the task, so that it can finish.
  unblock_task.Set();

  // In a short while CancelAll should return
  EXPECT_TRUE(canceled.Wait(/*ms=*/10));
}

TEST(CancelableTaskFactoryTest, StartedTaskBlocksMultipleCancelAll) {
  CancelableTaskFactory factory;
  rtc::Event task_started;
  rtc::Event unblock_task;
  std::unique_ptr<QueuedTask> task = factory.CreateTask([&] {
    task_started.Set();
    unblock_task.Wait(kAlmostForeverMs);
  });

  TaskQueueForTest thread1;
  thread1.PostTask(std::move(task));
  // Wait until task started before trying to cancel it.
  task_started.Wait(kAlmostForeverMs);

  // Cancel on a dedicated thread to avoid blocking main thread.
  TaskQueueForTest thread2;
  rtc::Event canceled2;
  thread2.PostTask(ToQueuedTask([&] {
    factory.CancelAll();
    canceled2.Set();
  }));

  // Cancel on another thread too.
  TaskQueueForTest thread3;
  rtc::Event canceled3;
  thread3.PostTask(ToQueuedTask([&] {
    factory.CancelAll();
    canceled3.Set();
  }));

  // Expect thread2 and thread3 are blocked.
  EXPECT_FALSE(canceled2.Wait(/*ms=*/50));
  // The main thread was already idling for 50ms, no need to double that time.
  EXPECT_FALSE(canceled3.Wait(/*ms=*/0));

  // Unblock the task, so that it can finish.
  unblock_task.Set();

  // In a short while CancelAll should return for both threads.
  EXPECT_TRUE(canceled2.Wait(/*ms=*/10));
  EXPECT_TRUE(canceled3.Wait(/*ms=*/0));
}

TEST(CancelableTaskFactoryTest, MultipleStartedTaskBlocksCancelAll) {
  CancelableTaskFactory factory;

  TaskQueueForTest thread1;
  rtc::Event task_started1;
  rtc::Event unblock_task1;
  thread1.PostTask(factory.CreateTask([&] {
    task_started1.Set();
    unblock_task1.Wait(kAlmostForeverMs);
  }));

  TaskQueueForTest thread2;
  rtc::Event task_started2;
  rtc::Event unblock_task2;
  thread2.PostTask(factory.CreateTask([&] {
    task_started2.Set();
    unblock_task2.Wait(kAlmostForeverMs);
  }));

  // Wait until task are started before trying to cancel it.
  task_started1.Wait(kAlmostForeverMs);
  task_started2.Wait(kAlmostForeverMs);

  // Cancel on a dedicated thread to avoid blocking main thread.
  TaskQueueForTest thread3;
  rtc::Event canceled;
  thread2.PostTask(ToQueuedTask([&] {
    factory.CancelAll();
    canceled.Set();
  }));

  // Expect thread that is trying to canel tasks is blocked.
  EXPECT_FALSE(canceled.Wait(/*ms=*/50));

  // Unblock one task, expect thread is still blocked because of the 2nd task.
  unblock_task1.Set();
  EXPECT_FALSE(canceled.Wait(/*ms=*/50));

  unblock_task2.Set();

  // In a short while CancelAll should return.
  EXPECT_TRUE(canceled.Wait(/*ms=*/10));
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(CancelableTaskFactoryDeathTest, CrashIfDestroyedBeforeCanceled) {
  absl::optional<CancelableTaskFactory> factory(absl::in_place);

  EXPECT_DEATH(factory = absl::nullopt, "");

  // Cleanup.
  factory->CancelAll();
}

// It will only crash in debug mode. In release mode it would cause a deadlock.
TEST(CancelableTaskFactoryDeathTest,
     CrashWhenTriesToCancelFromSelfCreatedTask) {
  CancelableTaskFactory factory;
  std::unique_ptr<QueuedTask> task =
      factory.CreateTask([&] { factory.CancelAll(); });

  EXPECT_DEATH(task->Run(), "");

  // Cleanup.
  factory.CancelAll();
}
#endif

}  // namespace
}  // namespace webrtc
