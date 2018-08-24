/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/cancelable_task_handler.h"

#include "rtc_base/event.h"
#include "system_wrappers/include/sleep.h"
#include "test/gmock.h"

namespace {

using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::webrtc::SleepMs;

TEST(CancelableTaskTest, CancelDoesnCrashOnEmptyObject) {
  rtc::CancelableTaskHandler handler;
  handler.Cancel();
}

TEST(CancelableTaskTest, CancelTaskBeforeItRun) {
  MockFunction<int()> closure;
  EXPECT_CALL(closure, Call()).Times(0);
  rtc::CancelableTaskHandler handler;
  rtc::TaskQueue task_queue("queue");
  handler.StartPeriodicTask(closure.AsStdFunction(), &task_queue, 20);
  SleepMs(10);
  handler.Cancel();
  SleepMs(20);
}

TEST(CancelableTaskTest, CancelTaskAfterItRunAndDeleted) {
  MockFunction<int()> closure;
  rtc::Event run(false, false);
  rtc::CancelableTaskHandler handler;
  rtc::TaskQueue task_queue("queue");
  handler.StartPeriodicTask(
      [&run] {
        EXPECT_FALSE(run.Wait(0));
        run.Set();
        return 0;
      },
      &task_queue, 10);
  SleepMs(20);
  EXPECT_TRUE(run.Wait(0));
  handler.Cancel();
}

TEST(CancelableTaskTest, CancelTaskUsingCopyBeforeItRun) {
  MockFunction<int()> closure;
  EXPECT_CALL(closure, Call()).Times(0);
  rtc::CancelableTaskHandler handler;
  rtc::TaskQueue task_queue("queue");
  handler.StartPeriodicTask(closure.AsStdFunction(), &task_queue, 20);
  rtc::CancelableTaskHandler copy = handler;
  SleepMs(10);
  copy.Cancel();
  SleepMs(20);
}

TEST(CancelableTaskTest, CancelNextTaskWhileRunningTask) {
  rtc::Event started(false, false);
  rtc::Event unpause(false, false);

  rtc::CancelableTaskHandler handler;
  rtc::TaskQueue task_queue("queue");
  handler.StartPeriodicTask(
      [&] {
        started.Set();
        EXPECT_TRUE(unpause.Wait(100));
        return 10;
      },
      &task_queue, 10);

  EXPECT_TRUE(started.Wait(100));
  handler.Cancel();
  unpause.Set();

  started.Reset();
  EXPECT_FALSE(started.Wait(100));
}

TEST(CancelableTaskTest, StopTaskQueueBeforeTaskRun) {
  MockFunction<int()> closure;
  EXPECT_CALL(closure, Call()).Times(0);
  rtc::CancelableTaskHandler handler;
  rtc::TaskQueue task_queue("queue");
  handler.StartPeriodicTask(closure.AsStdFunction(), &task_queue, 20);
}

TEST(CancelableTaskTest, StartOneTimeTask) {
  MockFunction<int()> closure;
  EXPECT_CALL(closure, Call()).WillOnce(Return(0));
  rtc::CancelableTaskHandler handler;
  rtc::TaskQueue task_queue("queue");
  handler.StartPeriodicTask(closure.AsStdFunction(), &task_queue, 20);
  SleepMs(50);
}

TEST(CancelableTaskTest, StartPeriodicTask) {
  MockFunction<int()> closure;
  rtc::Event done(false, false);
  EXPECT_CALL(closure, Call())
      .WillOnce(Return(20))
      .WillOnce(Return(20))
      .WillOnce(Invoke([&done] {
        done.Set();
        return 0;
      }));
  rtc::CancelableTaskHandler handler;
  rtc::TaskQueue task_queue("queue");
  handler.StartPeriodicTask(closure.AsStdFunction(), &task_queue, 0);
  EXPECT_TRUE(done.Wait(100));
}

TEST(CancelableTaskTest, DeletingHandlerDoesnStopTheTask) {
  rtc::Event run(false, false);
  auto handler = absl::make_unique<rtc::CancelableTaskHandler>();
  rtc::TaskQueue task_queue("queue");
  handler->StartPeriodicTask(
      [&] {
        run.Set();
        return 0;
      },
      &task_queue, 10);
  handler = nullptr;
  EXPECT_TRUE(run.Wait(100));
}

}  // namespace
