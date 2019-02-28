/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_UTILS_POST_TASK_H_
#define RTC_BASE_TASK_UTILS_POST_TASK_H_

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "api/task_queue/queued_task.h"

namespace webrtc {
namespace webrtC_post_task_impl {
// Simple implementation of QueuedTask for use with rtc::Bind and lambdas.
template <typename Closure>
class ClosureTask : public QueuedTask {
 public:
  explicit ClosureTask(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}

 private:
  bool Run() override {
    closure_();
    return true;
  }

  typename std::decay<Closure>::type closure_;
};

// Extends ClosureTask to also allow specifying cleanup code.
// This is useful when using lambdas if guaranteeing cleanup, even if a task
// was dropped (queue is too full), is required.
template <typename Closure, typename Cleanup>
class ClosureTaskWithCleanup : public ClosureTask<Closure> {
 public:
  ClosureTaskWithCleanup(Closure&& closure, Cleanup&& cleanup)
      : ClosureTask<Closure>(std::forward<Closure>(closure)),
        cleanup_(std::forward<Cleanup>(cleanup)) {}
  ~ClosureTaskWithCleanup() override { cleanup_(); }

 private:
  typename std::decay<Cleanup>::type cleanup_;
};

}  // namespace webrtC_post_task_impl

// Convenience function to construct closures that can be passed directly
// to methods that support std::unique_ptr<QueuedTask> but not template
// based parameters.
template <typename Closure>
std::unique_ptr<QueuedTask> NewClosure(Closure&& closure) {
  return absl::make_unique<webrtC_post_task_impl::ClosureTask<Closure>>(
      std::forward<Closure>(closure));
}

template <typename Closure, typename Cleanup>
std::unique_ptr<QueuedTask> NewClosure(Closure&& closure, Cleanup&& cleanup) {
  return absl::make_unique<
      webrtC_post_task_impl::ClosureTaskWithCleanup<Closure, Cleanup>>(
      std::forward<Closure>(closure), std::forward<Cleanup>(cleanup));
}

// Convenience functions to Post a Task or functor (e.g. lambda) to a
// TaskQueueBase interface.
template <typename TaskQueuePtr>
void PostTask(const TaskQueuePtr& task_queue,
              std::unique_ptr<QueuedTask> task) {
  task_queue->PostTask(std::move(task));
}

template <typename TaskQueuePtr,
          typename Closure,
          typename std::enable_if<!std::is_convertible<
              Closure,
              std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
void PostTask(const TaskQueuePtr& task_queue, Closure&& closure) {
  task_queue->PostTask(NewClosure(std::forward<Closure>(closure)));
}

template <typename TaskQueuePtr, typename Closure, typename Cleanup>
void PostTask(const TaskQueuePtr& task_queue,
              Closure&& closure,
              Cleanup&& cleanup) {
  task_queue->PostTask(NewClosure(std::forward<Closure>(closure),
                                  std::forward<Cleanup>(cleanup)));
}

// Convenience functions to Post a delayed Task or functor (e.g. lambda) to a
// TaskQueueBase interface.
template <typename TaskQueuePtr>
void PostDelayedTask(const TaskQueuePtr& task_queue,
                     std::unique_ptr<QueuedTask> task,
                     uint32_t delay_ms) {
  task_queue->PostDelayedTask(std::move(task), delay_ms);
}

template <typename TaskQueuePtr,
          typename Closure,
          typename std::enable_if<!std::is_convertible<
              Closure,
              std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
void PostDelayedTask(const TaskQueuePtr& task_queue,
                     Closure&& closure,
                     uint32_t delay_ms) {
  task_queue->PostDelayedTask(NewClosure(std::forward<Closure>(closure)),
                              delay_ms);
}

template <typename TaskQueuePtr, typename Closure, typename Cleanup>
void PostDelayedTask(const TaskQueuePtr& task_queue,
                     Closure&& closure,
                     Cleanup&& cleanup,
                     uint32_t delay_ms) {
  task_queue->PostDelayedTask(NewClosure(std::forward<Closure>(closure),
                                         std::forward<Cleanup>(cleanup)),
                              delay_ms);
}

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_POST_TASK_H_
