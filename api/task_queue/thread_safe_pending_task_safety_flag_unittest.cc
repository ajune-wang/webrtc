/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/task_queue/thread_safe_pending_task_safety_flag.h"

#include <memory>
#include <utility>

#include "api/units/time_delta.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::MockFunction;

constexpr TimeDelta kAlmostForever = TimeDelta::Seconds(1);

TEST(ThreadSafePendingTaskSafetyFlagTest, CanRunTaskBeforeCanceled) {
  ThreadSafePendingTaskSafetyFlag safety;
  bool run = false;
  absl::AnyInvocable<void()&&> task = safety.WrapTask([&run] { run = true; });

  std::move(task)();

  EXPECT_TRUE(run);

  // Cleanup
  safety.SetNotAlive();
}

TEST(ThreadSafePendingTaskSafetyFlagTest, DoesntRunTaskAfterCanceled) {
  ThreadSafePendingTaskSafetyFlag safety;
  bool run = false;
  absl::AnyInvocable<void()&&> task = safety.WrapTask([&run] { run = true; });
  safety.SetNotAlive();

  std::move(task)();

  EXPECT_FALSE(run);
}

TEST(ThreadSafePendingTaskSafetyFlagTest, DoesntRunTaskCreatedAfterCanceled) {
  ThreadSafePendingTaskSafetyFlag safety;
  safety.SetNotAlive();
  bool run = false;
  absl::AnyInvocable<void()&&> task = safety.WrapTask([&run] { run = true; });

  std::move(task)();

  EXPECT_FALSE(run);
}

TEST(ThreadSafePendingTaskSafetyFlagTest, StartedTaskBlocksCancelAll) {
  ThreadSafePendingTaskSafetyFlag safety;
  rtc::Event task_started;
  rtc::Event unblock_task;
  absl::AnyInvocable<void()&&> task = safety.WrapTask([&] {
    task_started.Set();
    unblock_task.Wait(kAlmostForever);
  });

  TaskQueueForTest thread1;
  thread1.PostTask(std::move(task));
  // Wait until task started before trying to cancel it.
  task_started.Wait(kAlmostForever);

  // Cancel on a dedicated thread to avoid blocking main thread.
  TaskQueueForTest thread2;
  rtc::Event canceled;
  thread2.PostTask([&] {
    safety.SetNotAlive();
    canceled.Set();
  });

  // Expect thread2 is blocked.
  EXPECT_FALSE(canceled.Wait(TimeDelta::Millis(50)));

  // Unblock the task, so that it can finish.
  unblock_task.Set();

  // In a short while SetNotAlive should return
  EXPECT_TRUE(canceled.Wait(TimeDelta::Millis(10)));
}

TEST(ThreadSafePendingTaskSafetyFlagTest, StartedTaskBlocksMultipleCancelAll) {
  ThreadSafePendingTaskSafetyFlag safety;
  rtc::Event task_started;
  rtc::Event unblock_task;
  absl::AnyInvocable<void()&&> task = safety.WrapTask([&] {
    task_started.Set();
    unblock_task.Wait(kAlmostForever);
  });

  TaskQueueForTest thread1;
  thread1.PostTask(std::move(task));
  // Wait until task started before trying to cancel it.
  task_started.Wait(kAlmostForever);

  // Cancel on a dedicated thread to avoid blocking main thread.
  TaskQueueForTest thread2;
  rtc::Event canceled2;
  thread2.PostTask([&] {
    safety.SetNotAlive();
    canceled2.Set();
  });

  // Cancel on another thread too.
  TaskQueueForTest thread3;
  rtc::Event canceled3;
  thread3.PostTask([&] {
    safety.SetNotAlive();
    canceled3.Set();
  });

  // Expect thread2 and thread3 are blocked.
  EXPECT_FALSE(canceled2.Wait(TimeDelta::Millis(50)));
  // The main thread was already idling for 50ms, no need to double that time.
  EXPECT_FALSE(canceled3.Wait(TimeDelta::Zero()));

  // Unblock the task, so that it can finish.
  unblock_task.Set();

  // In a short while SetNotAlive should return for both threads.
  EXPECT_TRUE(canceled2.Wait(TimeDelta::Millis(10)));
  EXPECT_TRUE(canceled3.Wait(TimeDelta::Zero()));
}

TEST(ThreadSafePendingTaskSafetyFlagTest, MultipleStartedTaskBlocksCancelAll) {
  ThreadSafePendingTaskSafetyFlag safety;

  TaskQueueForTest thread1;
  rtc::Event task_started1;
  rtc::Event unblock_task1;
  thread1.PostTask(safety.WrapTask([&] {
    task_started1.Set();
    unblock_task1.Wait(kAlmostForever);
  }));

  TaskQueueForTest thread2;
  rtc::Event task_started2;
  rtc::Event unblock_task2;
  thread2.PostTask(safety.WrapTask([&] {
    task_started2.Set();
    unblock_task2.Wait(kAlmostForever);
  }));

  // Wait until task are started before trying to cancel it.
  task_started1.Wait(kAlmostForever);
  task_started2.Wait(kAlmostForever);

  // Cancel on a dedicated thread to avoid blocking main thread.
  TaskQueueForTest thread3;
  rtc::Event canceled;
  thread2.PostTask([&] {
    safety.SetNotAlive();
    canceled.Set();
  });

  // Expect thread that is trying to canel tasks is blocked.
  EXPECT_FALSE(canceled.Wait(TimeDelta::Millis(50)));

  // Unblock one task, expect thread is still blocked because of the 2nd task.
  unblock_task1.Set();
  EXPECT_FALSE(canceled.Wait(TimeDelta::Millis(50)));

  unblock_task2.Set();

  // In a short while SetNotAlive should return.
  EXPECT_TRUE(canceled.Wait(TimeDelta::Millis(10)));
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(ThreadSafePendingTaskSafetyFlagDeathTest, CrashIfDestroyedBeforeCanceled) {
  absl::optional<ThreadSafePendingTaskSafetyFlag> safety(absl::in_place);

  EXPECT_DEATH(safety = absl::nullopt, "");

  // Cleanup.
  safety->SetNotAlive();
}
#endif

}  // namespace
}  // namespace webrtc
