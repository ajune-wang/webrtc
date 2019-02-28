/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/new_closure.h"

#include <memory>

#include "absl/memory/memory.h"
#include "api/task_queue/queued_task.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::InSequence;
using ::testing::MockFunction;

void RunTask(std::unique_ptr<QueuedTask> task) {
  // Simulate how task queue suppose to run tasks.
  QueuedTask* raw = task.release();
  if (raw->Run())
    delete raw;
}

TEST(NewClosureTest, AcceptsLambda) {
  bool run = false;
  std::unique_ptr<QueuedTask> task = NewClosure([&run] { run = true; });
  EXPECT_FALSE(run);
  RunTask(std::move(task));
  EXPECT_TRUE(run);
}

TEST(NewClosureTest, AcceptsCopyableClosure) {
  struct CopyableClosure {
    CopyableClosure(int* num_copies, int* num_moves, int* num_runs)
        : num_copies(num_copies), num_moves(num_moves), num_runs(num_runs) {}
    CopyableClosure(const CopyableClosure& other)
        : num_copies(other.num_copies),
          num_moves(other.num_moves),
          num_runs(other.num_runs) {
      ++*num_copies;
    }
    CopyableClosure(CopyableClosure&& other)
        : num_copies(other.num_copies),
          num_moves(other.num_moves),
          num_runs(other.num_runs) {
      ++*num_moves;
    }
    void operator()() { ++*num_runs; }

    int* num_copies;
    int* num_moves;
    int* num_runs;
  };

  int num_copies = 0;
  int num_moves = 0;
  int num_runs = 0;

  std::unique_ptr<QueuedTask> task;
  {
    CopyableClosure closure(&num_copies, &num_moves, &num_runs);
    task = NewClosure(closure);
    // Destroy closure to check with msan and tsan posted task has own copy.
  }
  EXPECT_EQ(num_copies, 1);
  RunTask(std::move(task));
  EXPECT_EQ(num_copies, 1);
  EXPECT_EQ(num_moves, 0);
  EXPECT_EQ(num_runs, 1);
}

TEST(NewClosureTest, AcceptsMoveOnlyClosure) {
  struct SomeState {
    explicit SomeState(bool* deleted) : deleted(deleted) {}
    ~SomeState() { *deleted = true; }
    bool* deleted;
  };
  struct MoveOnlyClosure {
    MoveOnlyClosure(int* num_moves, std::unique_ptr<SomeState> state)
        : num_moves(num_moves), state(std::move(state)) {}
    MoveOnlyClosure(const MoveOnlyClosure&) = delete;
    MoveOnlyClosure(MoveOnlyClosure&& other)
        : num_moves(other.num_moves), state(std::move(other.state)) {
      ++*num_moves;
    }
    void operator()() { state.reset(); }

    int* num_moves;
    std::unique_ptr<SomeState> state;
  };

  int num_moves = 0;
  bool state_deleted = false;
  auto state = absl::make_unique<SomeState>(&state_deleted);

  auto task = NewClosure(MoveOnlyClosure(&num_moves, std::move(state)));
  EXPECT_EQ(num_moves, 1);
  RunTask(std::move(task));

  EXPECT_TRUE(state_deleted);
  EXPECT_EQ(num_moves, 1);
}

TEST(NewClosureTest, AcceptsMoveOnlyCleanup) {
  struct SomeState {
    explicit SomeState(std::function<void()> trigger)
        : trigger(std::move(trigger)) {}
    ~SomeState() { trigger(); }
    std::function<void()> trigger;
  };
  struct MoveOnlyClosure {
    void operator()() { state.reset(); }

    std::unique_ptr<SomeState> state;
  };

  MockFunction<void()> run;
  MockFunction<void()> cleanup;
  auto state_run = absl::make_unique<SomeState>(run.AsStdFunction());
  auto state_cleanup = absl::make_unique<SomeState>(cleanup.AsStdFunction());

  auto task = NewClosure(MoveOnlyClosure{std::move(state_run)},
                         MoveOnlyClosure{std::move(state_cleanup)});

  // Expect run closure to complete before cleanup closure.
  InSequence in_sequence;
  EXPECT_CALL(run, Call);
  EXPECT_CALL(cleanup, Call);
  RunTask(std::move(task));
}

}  // namespace
}  // namespace webrtc
