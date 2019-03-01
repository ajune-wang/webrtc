/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/task_queue/task_queue_base.h"

#include <algorithm>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/time_utils.h"

#if defined(ABSL_HAVE_THREAD_LOCAL)

namespace webrtc {
namespace {

ABSL_CONST_INIT thread_local TaskQueueBase* current = nullptr;

class RepeatingTaskWrapper : public QueuedTask,
                             public TaskQueueBase::TaskHandleBase {
 public:
  RepeatingTaskWrapper(TaskQueueBase* task_queue,
                       TimeDelta initial_delay,
                       std::unique_ptr<QueuedRepeatingTask> task)
      : task_queue_(task_queue),
        next_run_time_(Timestamp::us(rtc::TimeMicros()) + initial_delay),
        task_(std::move(task)) {}

  bool Run() override {
    // Return true to tell the TaskQueue to destruct this object.
    if (next_run_time_.IsPlusInfinity())
      return true;

    TimeDelta delay = task_->Run(Timestamp::us(rtc::TimeMicros()));

    // The closure might have stopped this task, in which case we return true to
    // destruct this object.
    if (next_run_time_.IsPlusInfinity())
      return true;

    // We do not allow the task to destroy itself since that would invalidate
    // any existing references to the task.
    RTC_DCHECK(delay.IsFinite());

    TimeDelta lost_time = Timestamp::us(rtc::TimeMicros()) - next_run_time_;
    next_run_time_ += delay;
    delay -= lost_time;
    delay = std::max(delay, TimeDelta::Zero());

    task_queue_->PostDelayedTask(absl::WrapUnique(this), delay.ms());

    // Return false to tell the TaskQueue to not destruct this object since we
    // have taken ownership with absl::WrapUnique.
    return false;
  }

  void Stop() override {
    RTC_DCHECK(task_queue_->IsCurrent());
    RTC_DCHECK(next_run_time_.IsFinite());
    next_run_time_ = Timestamp::PlusInfinity();
  }

 private:
  TaskQueueBase* const task_queue_;
  Timestamp next_run_time_;
  std::unique_ptr<QueuedRepeatingTask> task_;
};
}  // namespace

TaskQueueBase::TaskHandleBase* TaskQueueBase::Repeat(
    TimeDelta initial_delay,
    std::unique_ptr<QueuedRepeatingTask> task) {
  auto handle = absl::make_unique<RepeatingTaskWrapper>(this, initial_delay,
                                                        std::move(task));
  TaskHandleBase* handle_ptr = handle.get();
  if (initial_delay == TimeDelta::Zero()) {
    PostTask(std::move(handle));
  } else {
    PostDelayedTask(std::move(handle), initial_delay.ms());
  }
  return handle_ptr;
}  // namespace

void TaskQueueBase::BlockingInvokeTask(std::unique_ptr<QueuedTask> task) {
  RTC_DCHECK(!IsCurrent());
  rtc::Event done;
  class InvokeWrapper : public QueuedTask {
   public:
    InvokeWrapper(std::unique_ptr<QueuedTask> task, rtc::Event* done)
        : task_(std::move(task)), done_(done) {}
    bool Run() override {
      bool res = task_->Run();
      RTC_CHECK_EQ(res, true);
      return res;
    }
    ~InvokeWrapper() override { done_->Set(); }
    std::unique_ptr<QueuedTask> task_;
    rtc::Event* done_;
  };
  PostTask(absl::make_unique<InvokeWrapper>(std::move(task), &done));
  done.Wait(rtc::Event::kForever);
}

TaskQueueBase* TaskQueueBase::Current() {
  return current;
}

TaskQueueBase::CurrentTaskQueueSetter::CurrentTaskQueueSetter(
    TaskQueueBase* task_queue)
    : previous_(current) {
  current = task_queue;
}

TaskQueueBase::CurrentTaskQueueSetter::~CurrentTaskQueueSetter() {
  current = previous_;
}
}  // namespace webrtc

#elif defined(WEBRTC_POSIX)

#include <pthread.h>

namespace webrtc {
namespace {

ABSL_CONST_INIT pthread_key_t g_queue_ptr_tls = 0;

void InitializeTls() {
  RTC_CHECK(pthread_key_create(&g_queue_ptr_tls, nullptr) == 0);
}

pthread_key_t GetQueuePtrTls() {
  static pthread_once_t init_once = PTHREAD_ONCE_INIT;
  RTC_CHECK(pthread_once(&init_once, &InitializeTls) == 0);
  return g_queue_ptr_tls;
}

}  // namespace

TaskQueueBase* TaskQueueBase::Current() {
  return static_cast<TaskQueueBase*>(pthread_getspecific(GetQueuePtrTls()));
}

TaskQueueBase::CurrentTaskQueueSetter::CurrentTaskQueueSetter(
    TaskQueueBase* task_queue)
    : previous_(TaskQueueBase::Current()) {
  pthread_setspecific(GetQueuePtrTls(), task_queue);
}

TaskQueueBase::CurrentTaskQueueSetter::~CurrentTaskQueueSetter() {
  pthread_setspecific(GetQueuePtrTls(), previous_);
}

}  // namespace webrtc

#else
#error Unsupported platform
#endif
