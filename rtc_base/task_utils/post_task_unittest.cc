/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/post_task.h"

#include <memory>

#include "absl/memory/memory.h"
#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::SaveArg;

// TaskQueue implementation that runs posted task as soon as they are posted,
// but also notify test about that via mock functions.
class MockTaskQueue : public TaskQueueBase {
 public:
  MockTaskQueue() {
    ON_CALL(*this, RunTask).WillByDefault([](QueuedTask* task) {
      if (task->Run())
        delete task;
    });
  }

  MOCK_METHOD1(PostTask, void(QueuedTask*));
  MOCK_METHOD2(PostDelayedTask, void(QueuedTask*, uint32_t));

  // Stub function to control how to run a task. By default runs it inline.
  MOCK_METHOD1(RunTask, void(QueuedTask*));

  void Delete() override { delete this; }
  void PostTask(std::unique_ptr<QueuedTask> task) override {
    QueuedTask* raw_task = task.release();
    PostTask(raw_task);
    RunTask(raw_task);
  }
  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t delay_ms) override {
    QueuedTask* raw_task = task.release();
    PostDelayedTask(raw_task, delay_ms);
    // Ignore the delay.
    RunTask(raw_task);
  }
};

class MockLifetime {
 public:
  MOCK_METHOD0(Copy, void());
  MOCK_METHOD0(Move, void());
  MOCK_METHOD0(Run, void());
};

struct CopyableClosure {
  explicit CopyableClosure(MockLifetime* lifetime) : lifetime(lifetime) {}
  CopyableClosure(const CopyableClosure& other) : lifetime(other.lifetime) {
    lifetime->Copy();
  }
  CopyableClosure(CopyableClosure&& other) : lifetime(other.lifetime) {
    lifetime->Move();
  }
  void operator()() { lifetime->Run(); }

  MockLifetime* lifetime;
};

struct MoveOnlyClosure {
  explicit MoveOnlyClosure(MockLifetime* lifetime) : lifetime(lifetime) {}
  MoveOnlyClosure(const MoveOnlyClosure& other) = delete;
  MoveOnlyClosure(MoveOnlyClosure&& other) : lifetime(other.lifetime) {
    lifetime->Move();
  }
  void operator()() { lifetime->Run(); }

  MockLifetime* lifetime;
};

class CustomTask : public QueuedTask {
 public:
  bool Run() override { return true; }
};

TEST(PostTaskTest, PostCustomTask) {
  auto task = absl::make_unique<CustomTask>();
  QueuedTask* raw = task.get();

  NiceMock<MockTaskQueue> queue;
  EXPECT_CALL(queue, PostTask(raw));
  PostTask(&queue, std::move(task));
}

TEST(PostTaskTest, PostFunction) {
  NiceMock<MockTaskQueue> queue;
  MockFunction<void()> function;

  EXPECT_CALL(function, Call);
  EXPECT_CALL(queue, PostTask);
  PostTask(&queue, function.AsStdFunction());
}

TEST(PostTaskTest, PostFunctionWithCleanup) {
  NiceMock<MockTaskQueue> queue;
  MockFunction<void()> function;
  MockFunction<void()> cleanup;

  EXPECT_CALL(queue, PostTask);
  InSequence in_sequence;
  EXPECT_CALL(function, Call);
  EXPECT_CALL(cleanup, Call);

  PostTask(&queue, function.AsStdFunction(), cleanup.AsStdFunction());
}

TEST(PostTaskTest, PostDelayedCustomTask) {
  auto task = absl::make_unique<CustomTask>();
  QueuedTask* raw = task.get();

  NiceMock<MockTaskQueue> queue;
  EXPECT_CALL(queue, PostDelayedTask(raw, 123u));
  PostDelayedTask(&queue, std::move(task), 123u);
}

TEST(PostTaskTest, PostDelayedFunction) {
  NiceMock<MockTaskQueue> queue;
  MockFunction<void()> function;

  EXPECT_CALL(queue, PostDelayedTask(_, 123));
  EXPECT_CALL(function, Call);
  PostDelayedTask(&queue, function.AsStdFunction(), 123);
}

TEST(PostTaskTest, PostDelayedFunctionWithCleanup) {
  NiceMock<MockTaskQueue> queue;
  MockFunction<void()> function;
  MockFunction<void()> cleanup;

  EXPECT_CALL(queue, PostDelayedTask(_, 123));
  InSequence in_sequence;
  EXPECT_CALL(function, Call);
  EXPECT_CALL(cleanup, Call);

  PostDelayedTask(&queue, function.AsStdFunction(), cleanup.AsStdFunction(),
                  123);
}

TEST(PostTaskTest, PostCopyableClosure) {
  MockLifetime closure;
  EXPECT_CALL(closure, Copy).Times(1);
  EXPECT_CALL(closure, Move).Times(0);
  EXPECT_CALL(closure, Run).Times(1);
  NiceMock<MockTaskQueue> post_queue;

  QueuedTask* task = nullptr;
  // Overwrite default behaviour: for this test it is important to run the task
  // after original closure is destroyed.
  EXPECT_CALL(post_queue, RunTask).WillOnce(SaveArg<0>(&task));
  {
    CopyableClosure copyable(&closure);
    PostTask(&post_queue, copyable);
    // Destroy closure to check with msan posted task has own copy.
  }

  ASSERT_TRUE(task);
  task->Run();
  delete task;
}

TEST(PostTaskTest, PostMoveOnlyClosure) {
  MockLifetime closure;
  EXPECT_CALL(closure, Move);
  EXPECT_CALL(closure, Run);

  NiceMock<MockTaskQueue> post_queue;
  PostTask(&post_queue, MoveOnlyClosure(&closure));
}

TEST(PostTaskTest, PostMoveOnlyCleanup) {
  NiceMock<MockLifetime> closure;
  NiceMock<MockLifetime> cleanup;

  InSequence in_sequence;
  EXPECT_CALL(closure, Run);
  EXPECT_CALL(cleanup, Run);

  NiceMock<MockTaskQueue> post_queue;
  PostTask(&post_queue, MoveOnlyClosure(&closure), MoveOnlyClosure(&cleanup));
}

}  // namespace
}  // namespace webrtc
