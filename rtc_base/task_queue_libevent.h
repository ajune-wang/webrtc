/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_TASK_QUEUE_LIBEVENT_H_
#define RTC_BASE_TASK_QUEUE_LIBEVENT_H_

#include <list>
#include <memory>

#include "absl/strings/string_view.h"
#include "api/task_queue/task_queue_base.h"
#include "base/third_party/libevent/event.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/platform_thread_types.h"

namespace webrtc {

class TaskQueueLibevent final : public TaskQueueBase {
 public:
  explicit TaskQueueLibevent(
      absl::string_view queue_name,
      rtc::ThreadPriority priority = rtc::kNormalPriority);
  void Delete() override;

  void PostTask(std::unique_ptr<QueuedTask> task) override;
  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override;

 private:
  class SetTimerTask;
  struct TimerEvent;

  ~TaskQueueLibevent() override;

  static void ThreadMain(void* context);
  static void OnWakeup(int socket, short flags, void* context);  // NOLINT
  static void RunTimer(int fd, short flags, void* context);      // NOLINT

  int wakeup_pipe_in_ = -1;
  int wakeup_pipe_out_ = -1;
  event_base* event_base_;
  std::unique_ptr<event> wakeup_event_;
  rtc::PlatformThread thread_;
  rtc::CriticalSection pending_lock_;
  std::list<std::unique_ptr<QueuedTask>> pending_ RTC_GUARDED_BY(pending_lock_);
  bool is_active_ = true;
  // Holds a list of events pending timers for cleanup when the loop exits.
  std::list<std::unique_ptr<TimerEvent>> pending_timers_;
};

}  // namespace webrtc

#endif  // RTC_BASE_TASK_QUEUE_LIBEVENT_H_
