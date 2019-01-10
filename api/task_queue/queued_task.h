/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TASK_QUEUE_QUEUED_TASK_H_
#define API_TASK_QUEUE_QUEUED_TASK_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "absl/memory/memory.h"

namespace webrtc {

// Base interface for asynchronously executed tasks.
// The interface basically consists of a single function, Run(), that executes
// on the target queue.  For more details see the Run() method and TaskQueue.
class QueuedTask {
 public:
  virtual ~QueuedTask() = default;

  // Main routine that will run when the task is executed on the desired queue.
  // The task should return |true| to indicate that it should be deleted or
  // |false| to indicate that the queue should consider ownership of the task
  // having been transferred.  Returning |false| can be useful if a task has
  // re-posted itself to a different queue or is otherwise being re-used.
  virtual bool Run() = 0;
};

namespace webrtc_task_queue_internal {
// Simple implementation of QueuedTask for use with callables.
template <class Closure>
class ClosureTask : public QueuedTask {
 public:
  explicit ClosureTask(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}

 private:
  bool Run() override {
    closure_();
    return true;
  }

  typename std::remove_const<
      typename std::remove_reference<Closure>::type>::type closure_;
};

// Extends ClosureTask to also allow specifying cleanup code.
// This is useful when using lambdas if guaranteeing cleanup, even if a task
// was dropped (queue is too full), is required.
template <class Closure, class Cleanup>
class ClosureTaskWithCleanup : public ClosureTask<Closure> {
 public:
  ClosureTaskWithCleanup(Closure&& closure, Cleanup&& cleanup)
      : ClosureTask<Closure>(std::forward<Closure>(closure)),
        cleanup_(std::forward<Cleanup>(cleanup)) {}
  ~ClosureTaskWithCleanup() { cleanup_(); }

 private:
  typename std::remove_const<
      typename std::remove_reference<Cleanup>::type>::type cleanup_;
};
}  // namespace webrtc_task_queue_internal

// Convenience function to construct closures that can be passed directly
// to methods that support std::unique_ptr<QueuedTask>.
template <typename Closure,
          typename std::enable_if<!std::is_convertible<
              Closure,
              std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
std::unique_ptr<QueuedTask> NewClosure(Closure&& closure) {
  return absl::make_unique<webrtc_task_queue_internal::ClosureTask<Closure>>(
      std::forward<Closure>(closure));
}

template <class Closure, class Cleanup>
std::unique_ptr<QueuedTask> NewClosure(Closure&& closure, Cleanup&& cleanup) {
  return absl::make_unique<
      webrtc_task_queue_internal::ClosureTaskWithCleanup<Closure, Cleanup>>(
      std::forward<Closure>(closure), std::forward<Cleanup>(cleanup));
}

}  // namespace webrtc

#endif  // API_TASK_QUEUE_QUEUED_TASK_H_
