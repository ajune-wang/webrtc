/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_QUEUE_FOR_TEST_H_
#define RTC_BASE_TASK_QUEUE_FOR_TEST_H_

#include <utility>

#include "absl/strings/string_view.h"
#include "api/scoped_refptr.h"
#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/event.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
class TaskQueueForTest;
namespace webrtc_task_qeueue_for_test_impl {
struct CancellationBase {
 private:
  friend class ::webrtc::TaskQueueForTest;
  bool stopped = false;
};
using RefCancellation = rtc::RefCountedObject<CancellationBase>;
}  // namespace webrtc_task_qeueue_for_test_impl

class RTC_LOCKABLE TaskQueueForTest : public rtc::TaskQueue {
 public:
  using TaskId =
      rtc::scoped_refptr<webrtc_task_qeueue_for_test_impl::RefCancellation>;

  using rtc::TaskQueue::TaskQueue;
  explicit TaskQueueForTest(absl::string_view name = "TestQueue",
                            Priority priority = Priority::NORMAL);
  TaskQueueForTest(const TaskQueueForTest&) = delete;
  TaskQueueForTest& operator=(const TaskQueueForTest&) = delete;
  ~TaskQueueForTest() = default;

  // A convenience, test-only method that blocks the current thread while
  // a task executes on the task queue.
  // This variant is specifically for posting custom QueuedTask derived
  // implementations that tests do not want to pass ownership of over to the
  // task queue (i.e. the Run() method always returns |false|.).
  template <class Closure>
  void SendTask(Closure* task) {
    RTC_DCHECK(!IsCurrent());
    rtc::Event event;
    PostTask(ToQueuedTask(
        [&task] { RTC_CHECK_EQ(false, static_cast<QueuedTask*>(task)->Run()); },
        [&event] { event.Set(); }));
    event.Wait(rtc::Event::kForever);
  }

  // A convenience, test-only method that blocks the current thread while
  // a task executes on the task queue.
  template <class Closure>
  void SendTask(Closure&& task) {
    RTC_DCHECK(!IsCurrent());
    rtc::Event event;
    PostTask(
        ToQueuedTask(std::forward<Closure>(task), [&event] { event.Set(); }));
    event.Wait(rtc::Event::kForever);
  }

  template <class Closure>
  TaskId PostDelayedCancelableTask(Closure&& task, int64_t delay_ms) {
    struct Stoppable {
     public:
      void operator()() {
        if (token->stopped)
          return;
        closure();
      }
      typename std::decay<Closure>::type closure;
      TaskId token;
    };
    TaskId token = new webrtc_task_qeueue_for_test_impl::RefCancellation();
    PostDelayedTask(ToQueuedTask(Stoppable{std::forward<Closure>(task), token}),
                    delay_ms);
    return token;
  }

  void CancelTask(TaskId token) {
    if (IsCurrent()) {
      token->stopped = true;
    } else {
      SendTask([&token] { token->stopped = true; });
    }
  }
};

}  // namespace webrtc

#endif  // RTC_BASE_TASK_QUEUE_FOR_TEST_H_
