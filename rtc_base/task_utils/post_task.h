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
#include <type_traits>
#include <utility>

#include "absl/memory/memory.h"
#include "api/task_queue/queued_task.h"

namespace webrtc {
namespace webrtc_post_task_impl {
// Simple implementation of QueuedTask for use with rtc::Bind and lambdas.
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

}  // namespace webrtc_post_task_impl

template <typename TaskQueueBasePtr>
void PostTask(const TaskQueueBasePtr& task_queue,
              std::unique_ptr<QueuedTask> task) {
  task_queue->PostTask(std::move(task));
}

template <typename TaskQueueBasePtr,
          typename Closure,
          typename std::enable_if<!std::is_convertible<
              Closure,
              std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
void PostTask(const TaskQueueBasePtr& task_queue, Closure&& task) {
  task_queue->PostTask(
      absl::make_unique<webrtc_post_task_impl::ClosureTask<Closure>>(
          std::forward<Closure>(task)));
}

template <typename TaskQueueBasePtr, typename Closure, typename Cleanup>
void PostTask(const TaskQueueBasePtr& task_queue,
              Closure&& task,
              Cleanup&& cleanup) {
  task_queue->PostTask(
      absl::make_unique<
          webrtc_post_task_impl::ClosureTaskWithCleanup<Closure, Cleanup>>(
          std::forward<Closure>(task), std::forward<Cleanup>(cleanup)));
}

template <typename TaskQueueBasePtr>
void PostDelayedTask(const TaskQueueBasePtr& task_queue,
                     std::unique_ptr<QueuedTask> task,
                     uint32_t delay_ms) {
  task_queue->PostDelayedTask(std::move(task), delay_ms);
}

template <typename TaskQueueBasePtr,
          typename Closure,
          typename std::enable_if<!std::is_convertible<
              Closure,
              std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
void PostDelayedTask(const TaskQueueBasePtr& task_queue,
                     Closure&& task,
                     uint32_t delay_ms) {
  task_queue->PostDelayedTask(
      absl::make_unique<webrtc_post_task_impl::ClosureTask<Closure>>(
          std::forward<Closure>(task)),
      delay_ms);
}

template <typename TaskQueueBasePtr, typename Closure, typename Cleanup>
void PostDelayedTask(const TaskQueueBasePtr& task_queue,
                     Closure&& task,
                     Cleanup&& cleanup,
                     uint32_t delay_ms) {
  task_queue->PostDelayedTask(
      absl::make_unique<
          webrtc_post_task_impl::ClosureTaskWithCleanup<Closure, Cleanup>>(
          std::forward<Closure>(task), std::forward<Cleanup>(cleanup)),
      delay_ms);
}

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_POST_TASK_H_
