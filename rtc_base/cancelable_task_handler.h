/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CANCELABLE_TASK_HANDLER_H_
#define RTC_BASE_CANCELABLE_TASK_HANDLER_H_

#include <functional>

#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/task_queue.h"

namespace rtc {

// This class is thread safe.
class CancelableTaskHandler {
 public:
  // This class is copyable and cheaply movable.
  CancelableTaskHandler();
  CancelableTaskHandler(const CancelableTaskHandler&);
  CancelableTaskHandler(CancelableTaskHandler&&);
  CancelableTaskHandler& operator=(const CancelableTaskHandler&);
  CancelableTaskHandler& operator=(CancelableTaskHandler&&);
  // Deleteing the handler doesn't Cancel the task.
  ~CancelableTaskHandler();

  // Starts a new |task| on |task_queue| in |initial_delay_ms| or immidiately if
  // delay is zero.
  // task() should return time in ms until next run or zero if task shouldn't
  // run again.
  // Note that starting new task doesn't cancel old one.
  void StartPeriodicTask(std::function<int()> task,
                         rtc::TaskQueue* task_queue,
                         int initial_delay_ms);

  // Prevents scheduling new runnings of task() started by StartPeriodicTask.
  // Doesn't wait if task() is already running.
  void Cancel();

 private:
  class CancelationToken;
  class Task;

  rtc::scoped_refptr<CancelationToken> cancelation_token_;
};

}  // namespace rtc

#endif  // RTC_BASE_CANCELABLE_TASK_HANDLER_H_
