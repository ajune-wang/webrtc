/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_QUEUE_H_
#define RTC_BASE_TASK_QUEUE_H_

#include <stdint.h>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread_annotations.h"

namespace rtc {
class TaskQueue;
}
namespace webrtc {
namespace task_queue_impl {
// Represents a task that can be stopped.
class StoppableTaskWrapper : public SequencedTask {
 public:
  explicit StoppableTaskWrapper(std::unique_ptr<SequencedTask> task);
  // Stops the task, called at most once. Must be called on the task queue
  // that owns the task that is being stopped.
  void Stop();

  // Implements SequencedTask
  TimeDelta Run(Timestamp at_time) override;
  // Destroyed by the owning task queue after being stopped or on destruction
  // of the task queue.
  virtual ~StoppableTaskWrapper() = default;

 private:
  std::unique_ptr<SequencedTask> task_;
};

template <class Closure>
class RepeatingTaskWrapper;

template <class Closure>
class RepeatingTaskWrapper<TimeDelta (Closure::*)(Timestamp) const>
    : public SequencedTask {
 public:
  explicit RepeatingTaskWrapper(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  TimeDelta Run(Timestamp at_time) override { return closure_(at_time); }
  typename std::decay<Closure>::type closure_;
};

template <class Closure>
class RepeatingTaskWrapper<TimeDelta (Closure::*)(Timestamp)>
    : public SequencedTask {
 public:
  explicit RepeatingTaskWrapper(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  TimeDelta Run(Timestamp at_time) override { return closure_(at_time); }
  typename std::decay<Closure>::type closure_;
};

template <class Closure>
class RepeatingTaskWrapper<TimeDelta (Closure::*)() const>
    : public SequencedTask {
 public:
  explicit RepeatingTaskWrapper(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  TimeDelta Run(Timestamp at_time) override { return closure_(); }
  typename std::decay<Closure>::type closure_;
};

template <class Closure>
class RepeatingTaskWrapper<TimeDelta (Closure::*)()> : public SequencedTask {
 public:
  explicit RepeatingTaskWrapper(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  TimeDelta Run(Timestamp at_time) override { return closure_(); }
  typename std::decay<Closure>::type closure_;
};

}  // namespace task_queue_impl

// Represents a repeating task that can be stopped. When it has been assigned a
// task it is in the running stage. It's always ok to call Stop, but it will not
// do anything in the non-running state. All members must be called from the
// task queue the target is running on.
class RepeatingTaskHandle {
 public:
  RepeatingTaskHandle();
  RepeatingTaskHandle(RepeatingTaskHandle&& other);
  RepeatingTaskHandle(const RepeatingTaskHandle&) = delete;
  RepeatingTaskHandle& operator=(RepeatingTaskHandle&& other);
  RepeatingTaskHandle& operator=(const RepeatingTaskHandle&) = delete;
  ~RepeatingTaskHandle();
  // Stops the task, must be called from the same task queue it's running on.
  void Stop();
  // Indicates that this task is running and has not been stopped.
  bool Running() const;

 private:
  friend class rtc::TaskQueue;
  explicit RepeatingTaskHandle(task_queue_impl::StoppableTaskWrapper* handle);
  task_queue_impl::StoppableTaskWrapper* handle_ = nullptr;
};
}  // namespace webrtc
namespace rtc {

// TODO(danilchap): Remove the alias when all of webrtc is updated to use
// webrtc::QueuedTask directly.
using ::webrtc::QueuedTask;

// Implements a task queue that asynchronously executes tasks in a way that
// guarantees that they're executed in FIFO order and that tasks never overlap.
// Tasks may always execute on the same worker thread and they may not.
// To DCHECK that tasks are executing on a known task queue, use IsCurrent().
//
// Here are some usage examples:
//
//   1) Asynchronously running a lambda:
//
//     class MyClass {
//       ...
//       TaskQueue queue_("MyQueue");
//     };
//
//     void MyClass::StartWork() {
//       queue_.PostTask([]() { Work(); });
//     ...
//
//   2) Posting a custom task on a timer.  The task posts itself again after
//      every running:
//
//     class TimerTask : public QueuedTask {
//      public:
//       TimerTask() {}
//      private:
//       bool Run() override {
//         ++count_;
//         TaskQueue::Current()->PostDelayedTask(
//             std::unique_ptr<QueuedTask>(this), 1000);
//         // Ownership has been transferred to the next occurance,
//         // so return false to prevent from being deleted now.
//         return false;
//       }
//       int count_ = 0;
//     };
//     ...
//     queue_.PostDelayedTask(
//         std::unique_ptr<QueuedTask>(new TimerTask()), 1000);
//
// For more examples, see task_queue_unittests.cc.
//
// A note on destruction:
//
// When a TaskQueue is deleted, pending tasks will not be executed but they will
// be deleted.  The deletion of tasks may happen asynchronously after the
// TaskQueue itself has been deleted or it may happen synchronously while the
// TaskQueue instance is being deleted.  This may vary from one OS to the next
// so assumptions about lifetimes of pending tasks should not be made.
class RTC_LOCKABLE RTC_EXPORT TaskQueue {
  using StoppableTaskWrapper = webrtc::task_queue_impl::StoppableTaskWrapper;
  template <typename Closure>
  using RepeatingTaskWrapper =
      webrtc::task_queue_impl::RepeatingTaskWrapper<Closure>;

 public:
  // TaskQueue priority levels. On some platforms these will map to thread
  // priorities, on others such as Mac and iOS, GCD queue priorities.
  using Priority = ::webrtc::TaskQueueFactory::Priority;

  explicit TaskQueue(std::unique_ptr<webrtc::TaskQueueBase,
                                     webrtc::TaskQueueDeleter> task_queue);
  explicit TaskQueue(const char* queue_name,
                     Priority priority = Priority::NORMAL);
  ~TaskQueue();

  static TaskQueue* Current();

  // Used for DCHECKing the current queue.
  bool IsCurrent() const;

  // Returns non-owning pointer to the task queue implementation.
  webrtc::TaskQueueBase* Get() { return impl_; }

  // TODO(tommi): For better debuggability, implement RTC_FROM_HERE.

  // Ownership of the task is passed to PostTask.
  void PostTask(std::unique_ptr<QueuedTask> task);

  // Posts a task and waits for it to finish before continuing. This can incur a
  // large runtime cost and if it is called from another task on a thread pool
  // it can cause a deadlock. Be careful when using this and ask for advice if
  // you think you need to add this in a new place but feel uncertain about the
  // implications.
  void BlockingInvokeTask(std::unique_ptr<QueuedTask> task);

  // Schedules a task to execute a specified number of milliseconds from when
  // the call is made. The precision should be considered as "best effort"
  // and in some cases, such as on Windows when all high precision timers have
  // been used up, can be off by as much as 15 millseconds (although 8 would be
  // more likely). This can be mitigated by limiting the use of delayed tasks.
  void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds);

  // std::enable_if is used here to make sure that calls to PostTask() with
  // std::unique_ptr<SomeClassDerivedFromQueuedTask> would not end up being
  // caught by this template.
  template <class Closure,
            typename std::enable_if<!std::is_convertible<
                Closure,
                std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
  void PostTask(Closure&& closure) {
    PostTask(webrtc::ToQueuedTask(std::forward<Closure>(closure)));
  }

  template <class Closure,
            typename std::enable_if<!std::is_convertible<
                Closure,
                std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
  void BlockingInvokeTask(Closure&& closure) {
    BlockingInvokeTask(webrtc::ToQueuedTask(std::forward<Closure>(closure)));
  }

  // See documentation above for performance expectations.
  template <class Closure,
            typename std::enable_if<!std::is_convertible<
                Closure,
                std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
  void PostDelayedTask(Closure&& closure, uint32_t milliseconds) {
    PostDelayedTask(webrtc::ToQueuedTask(std::forward<Closure>(closure)),
                    milliseconds);
  }
  // Posts a task to repeat |closure| on the underlying task queue.
  // The task will be repeated with a delay indicated by the TimeDelta return
  // value of |closure|. |closure| can optionally receive a Timestamp indicating
  // the time when it's run.
  template <class Closure, class CallSignature = decltype(&Closure::operator())>
  webrtc::RepeatingTaskHandle PostRepeatingTask(Closure&& closure) {
    return CreateRepeatHandle(
        webrtc::TimeDelta::Zero(),
        absl::make_unique<RepeatingTaskWrapper<CallSignature>>(
            std::forward<Closure>(closure)));
  }

  // Posts a task to repeat |closure| on the underlying task queue after the
  // given |delay| has passed. The task will be repeated with a delay indicated
  // by the TimeDelta return value of |closure|. |closure| can optionally
  // receive a Timestamp indicating the time when it's run.
  template <class Closure, class CallSignature = decltype(&Closure::operator())>
  webrtc::RepeatingTaskHandle PostDelayedRepeatingTask(
      webrtc::TimeDelta first_delay,
      Closure&& closure) {
    return CreateRepeatHandle(
        first_delay, absl::make_unique<RepeatingTaskWrapper<CallSignature>>(
                         std::forward<Closure>(closure)));
  }

 private:
  webrtc::RepeatingTaskHandle CreateRepeatHandle(
      webrtc::TimeDelta first_delay,
      std::unique_ptr<webrtc::SequencedTask> task) {
    auto stoppable = absl::make_unique<StoppableTaskWrapper>(std::move(task));
    auto* stoppable_ptr = stoppable.get();
    impl_->PostRepeatingTask(first_delay, std::move(stoppable));
    return webrtc::RepeatingTaskHandle(stoppable_ptr);
  }
  webrtc::TaskQueueBase* const impl_;

  RTC_DISALLOW_COPY_AND_ASSIGN(TaskQueue);
};

}  // namespace rtc

#endif  // RTC_BASE_TASK_QUEUE_H_
