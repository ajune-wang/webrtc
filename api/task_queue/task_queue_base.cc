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
#include "rtc_base/time_utils.h"

#if defined(ABSL_HAVE_THREAD_LOCAL)

namespace webrtc {
namespace {

ABSL_CONST_INIT thread_local TaskQueueBase* current = nullptr;

class RepeatingTaskWrapper : public QueuedTask {
 public:
  RepeatingTaskWrapper(TaskQueueBase* task_queue,
                       TimeDelta initial_delay,
                       std::unique_ptr<SequencedTask> task)
      : task_queue_(task_queue),
        next_run_time_(Timestamp::us(rtc::TimeMicros()) + initial_delay),
        task_(std::move(task)) {}

  bool Run() override {
    TimeDelta target_delay = task_->Run(Timestamp::us(rtc::TimeMicros()));
    // The task might have been stopped, in which case we return true to
    // destruct this object.
    if (target_delay.IsPlusInfinity())
      return true;
    TimeDelta lost_time = Timestamp::us(rtc::TimeMicros()) - next_run_time_;
    next_run_time_ += target_delay;
    TimeDelta delay = std::max(target_delay - lost_time, TimeDelta::Zero());
    task_queue_->PostDelayedTask(absl::WrapUnique(this), delay.ms());
    // Return false to tell the TaskQueue to not destruct this object since we
    // have taken ownership with absl::WrapUnique.
    return false;
  }

 private:
  TaskQueueBase* const task_queue_;
  Timestamp next_run_time_;
  std::unique_ptr<SequencedTask> task_;
};
}  // namespace

void TaskQueueBase::PostRepeatingTask(TimeDelta initial_delay,
                                      std::unique_ptr<SequencedTask> task) {
  auto handle = absl::make_unique<RepeatingTaskWrapper>(this, initial_delay,
                                                        std::move(task));
  if (initial_delay == TimeDelta::Zero()) {
    PostTask(std::move(handle));
  } else {
    PostDelayedTask(std::move(handle), initial_delay.ms());
  }
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
