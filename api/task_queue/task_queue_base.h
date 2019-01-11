/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TASK_QUEUE_TASK_QUEUE_BASE_H_
#define API_TASK_QUEUE_TASK_QUEUE_BASE_H_

#include <memory>
#include <utility>

#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base_impl.h"

namespace webrtc {

// TODO(danilchap): Remove inheritence from rtc::TaskQueue::Impl when custom
// implementations switch to use global factories that creates TaskQueueBase
// instead of using link-time injection.
class TaskQueueBase : public rtc::TaskQueue::Impl {
 public:
  // Starts destruction of the task queue. Responsible for deallocation.
  // Implementation may complete deallocation on a different thread after Stop
  // is returned. Once Stop is called, PostTask and PostDelayTask allowed to
  // be called only on the TaskQueue.
  // On return ensures no Task is running, nor any new task able to start
  // on the TaskQueue.
  virtual void Stop();

  void PostTask(std::unique_ptr<QueuedTask> task) override = 0;
  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override = 0;

  static TaskQueueBase* Current();
  bool IsCurrent() const { return Current() == this; }

  // std::enable_if is used here to make sure that calls to PostTask() with
  // std::unique_ptr<SomeClassDerivedFromQueuedTask> would not end up being
  // caught by this template.
  template <class Closure,
            typename std::enable_if<!std::is_convertible<
                Closure,
                std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
  void PostTask(Closure&& closure) {
    PostTask(NewClosure(std::forward<Closure>(closure)));
  }

  // See documentation above for performance expectations.
  template <class Closure,
            typename std::enable_if<!std::is_convertible<
                Closure,
                std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
  void PostDelayedTask(Closure&& closure, uint32_t milliseconds) {
    PostDelayedTask(NewClosure(std::forward<Closure>(closure)), milliseconds);
  }

 protected:
  class CurrentTaskQueueSetter {
   public:
    explicit CurrentTaskQueueSetter(TaskQueueBase* task_queue);
    CurrentTaskQueueSetter(const CurrentTaskQueueSetter&) = delete;
    CurrentTaskQueueSetter& operator=(const CurrentTaskQueueSetter&) = delete;
    ~CurrentTaskQueueSetter();

   private:
    TaskQueueBase* const previous_;
  };

  // Protected becaues users of the TaskQueueBase should call Stop instead of
  // directly deleting the object.
  ~TaskQueueBase() override = default;
};

struct TaskQueueDeleter {
  void operator()(TaskQueueBase* task_queue) const { task_queue->Stop(); }
};
using TaskQueuePtr = std::unique_ptr<TaskQueueBase, TaskQueueDeleter>;

template <typename TaskQueueType, typename... Args>
static TaskQueuePtr MakeTaskQueuePtr(Args&&... args) {
  return TaskQueuePtr(new TaskQueueType(std::forward<Args>(args)...));
}

}  // namespace webrtc

#endif  // API_TASK_QUEUE_TASK_QUEUE_BASE_H_
