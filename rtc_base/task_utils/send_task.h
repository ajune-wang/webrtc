/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_UTILS_SEND_TASK_H_
#define RTC_BASE_TASK_UTILS_SEND_TASK_H_

#include <type_traits>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

template <typename TaskQueuePtr, typename Closure>
void SendTask(const TaskQueuePtr& task_queue, Closure&& closure) {
  RTC_DCHECK(!task_queue->IsCurrent());
  rtc::Event task_done;
  task_queue->PostTask(rtc::NewClosure(std::forward<Closure>(closure),
                                       [&task_done] { task_done.Set(); }));
  task_done.Wait(rtc::Event::kForever);
}

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_SEND_TASK_H_
