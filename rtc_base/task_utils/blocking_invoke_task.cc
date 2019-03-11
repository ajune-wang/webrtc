/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/blocking_invoke_task.h"

#include "absl/memory/memory.h"
#include "rtc_base/event.h"

namespace webrtc {

void BlockingInvokeTask(TaskQueueBase* task_queue,
                        std::unique_ptr<QueuedTask> task) {
  RTC_DCHECK(!task_queue->IsCurrent());
  rtc::Event done;
  class InvokeWrapper : public QueuedTask {
   public:
    InvokeWrapper(std::unique_ptr<QueuedTask> task, rtc::Event* done)
        : task_(std::move(task)), done_(done) {}
    bool Run() override {
      bool delete_task = task_->Run();
      RTC_CHECK_EQ(delete_task, true);
      return true;
    }
    ~InvokeWrapper() override { done_->Set(); }
    std::unique_ptr<QueuedTask> task_;
    rtc::Event* done_;
  };
  task_queue->PostTask(
      absl::make_unique<InvokeWrapper>(std::move(task), &done));
  done.Wait(rtc::Event::kForever);
}

}  // namespace webrtc
