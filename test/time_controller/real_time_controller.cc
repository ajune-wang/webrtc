/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/time_controller/real_time_controller.h"

#include "api/task_queue/global_task_queue_factory.h"
#include "rtc_base/event.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "system_wrappers/include/sleep.h"

namespace webrtc {

Clock* RealTimeController::GetClock() {
  return Clock::GetRealTimeClock();
}

TaskQueueFactory* RealTimeController::GetTaskQueueFactory() {
  return &GlobalTaskQueueFactory();
}

std::unique_ptr<ProcessThread> RealTimeController::CreateProcessThread(
    const char* thread_name) {
  return ProcessThread::Create(thread_name);
}

void RealTimeController::Sleep(TimeDelta duration) {
  SleepMs(duration.ms());
}

void RealTimeController::SendTask(TaskQueueBase* task_queue,
                                  std::unique_ptr<QueuedTask> task) {
  RTC_DCHECK(!task_queue->IsCurrent());
  bool delete_task = true;
  rtc::Event event;
  task_queue->PostTask(
      ToQueuedTask([&task, &delete_task]() { delete_task = task->Run(); },
                   [&event]() { event.Set(); }));
  event.Wait(rtc::Event::kForever);
  if (!delete_task)
    task.release();
}

void RealTimeController::RunInEventScope(std::function<void()> closure) {
  closure();
}

}  // namespace webrtc
