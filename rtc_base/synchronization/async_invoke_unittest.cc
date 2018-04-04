/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/synchronization/async_invoke.h"

#include <utility>

#include "rtc_base/gunit.h"
#include "rtc_base/task_queue.h"

namespace rtc {
TEST(InvokeWaiterTest, CanWaitForChain) {
  TaskQueue tq1("TQ1");
  TaskQueue tq2("TQ2");
  TaskQueue tq3("TQ3");
  bool task3_done = false;
  InvokeWaiter waiter;
  struct Task3 {
    void operator()() { *done = true; }
    InvokeDoneBlocker blocker;
    bool* done;
  };
  struct Task2 {
    void operator()() {
      tq3->PostDelayedTask(Task3{std::move(blocker), done}, 5);
    }
    InvokeDoneBlocker blocker;
    TaskQueue* tq3;
    bool* done;
  };
  struct Task1 {
    void operator()() {
      tq2->PostDelayedTask(Task2{std::move(blocker), tq3, done}, 5);
    }
    InvokeDoneBlocker blocker;
    TaskQueue* tq2;
    TaskQueue* tq3;
    bool* done;
  };
  tq1.PostDelayedTask(Task1{waiter.CreateBlocker(), &tq2, &tq3, &task3_done},
                      10);
  EXPECT_FALSE(task3_done);
  waiter.Wait();
  EXPECT_TRUE(task3_done);
}

TEST(InvokeWaiterTest, CanWaitForFork) {
  TaskQueue tq1("TQ1");
  TaskQueue tq2("TQ2");
  TaskQueue tq3a("TQ3a");
  TaskQueue tq3b("TQ3b");
  bool task3a_done = false;
  bool task3b_done = false;
  InvokeWaiter waiter;
  struct Task3 {
    void operator()() { *done_ = true; }
    InvokeDoneBlocker blocker_;
    bool* done_;
  };
  struct Task2 {
    void operator()() {
      tq3_->PostDelayedTask(Task3{std::move(blocker_), done_}, 5);
    }
    InvokeDoneBlocker blocker_;
    TaskQueue* tq3_;
    bool* done_;
  };
  struct Task1 {
    void operator()() {
      tq2_->PostDelayedTask(Task2{blocker_, tq3a_, done_a_}, 5);
      tq2_->PostDelayedTask(Task2{std::move(blocker_), tq3b_, done_b_}, 3);
    }
    InvokeDoneBlocker blocker_;
    TaskQueue* tq2_;
    TaskQueue* tq3a_;
    TaskQueue* tq3b_;
    bool* done_a_;
    bool* done_b_;
  };
  tq1.PostDelayedTask(Task1{waiter.CreateBlocker(), &tq2, &tq3a, &tq3b,
                            &task3a_done, &task3b_done},
                      1);
  EXPECT_FALSE(task3a_done);
  EXPECT_FALSE(task3b_done);
  waiter.Wait();
  EXPECT_TRUE(task3a_done);
  EXPECT_TRUE(task3b_done);
}
}  // namespace rtc
