/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TASK_QUEUE_THREAD_SAFE_PENDING_TASK_SAFETY_FLAG_H_
#define API_TASK_QUEUE_THREAD_SAFE_PENDING_TASK_SAFETY_FLAG_H_

#include "absl/functional/any_invocable.h"
#include "api/scoped_refptr.h"

namespace webrtc {

// Wraps tasks to allow to cancell them without destroying the task queues
// they were posted to. Tasks guarded by this flag can be posted to different
// task queues.
// When all tasks are posted to and cancelled on the same task queue, it is
// recommended to use cheaper PendingTaskSafetyFlag.
// This class is thread safe.
class ThreadSafePendingTaskSafetyFlag {
 public:
  ThreadSafePendingTaskSafetyFlag();
  ThreadSafePendingTaskSafetyFlag(const ThreadSafePendingTaskSafetyFlag&) =
      delete;
  ThreadSafePendingTaskSafetyFlag& operator=(
      const ThreadSafePendingTaskSafetyFlag&) = delete;
  ~ThreadSafePendingTaskSafetyFlag();

  // Creates a task that invokes `task` or does nothing depending if SetNotAlive
  // was called. Created tasks can outlive `this`.
  absl::AnyInvocable<void() &&> WrapTask(absl::AnyInvocable<void() &&> task);

  // Disables running tasks created with `WrapTask`. If there are tasks
  // that are currently runnning, blocks current thread until those task are
  // complete. Tasks that haven't started before `SetNotAlive` was called will
  // become noop. It is allowed to create tasks after `SetNotAlive `is called,
  // but those tasks will be noop.
  // Must be called at least once.
  void SetNotAlive();

 private:
  struct Flag;
  rtc::scoped_refptr<Flag> flag_;
};

}  // namespace webrtc

#endif  // API_TASK_QUEUE_THREAD_SAFE_PENDING_TASK_SAFETY_FLAG_H_
