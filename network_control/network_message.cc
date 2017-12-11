/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "network_control/include/network_message.h"

namespace webrtc {
namespace network {
namespace signal {

PendingTaskHandler::PendingTask::PendingTask(closure_t&& closure,
                                             closure_t&& destroy,
                                             PendingTaskHandler* parent)
    : closure_(std::forward<closure_t>(closure)),
      destroy_(std::forward<closure_t>(destroy)),
      parent_(parent) {}

PendingTaskHandler::PendingTask::~PendingTask() {}

bool PendingTaskHandler::PendingTask::Run() {
  if (!depleted_) {
    depleted_ = true;
    closure_();
    destroy_();
    parent_->OnTaskDone(this);
    parent_ = nullptr;
  }
  return true;
}

void PendingTaskHandler::PendingTask::Cancel() {
  if (!depleted_) {
    depleted_ = true;
    destroy_();
    parent_ = nullptr;
  }
}

PendingTaskHandler::PendingTaskHandler(rtc::TaskQueue* target_queue)
    : target_queue_(target_queue) {}

PendingTaskHandler::~PendingTaskHandler() {
  {
    rtc::CritScope cs(&pending_lock_);
    stopped_ = true;
  }
  rtc::Event done(false, false);
  target_queue_->PostTask(rtc::NewClosure(
      [this] {
        rtc::CritScope cs(&pending_lock_);
        for (PendingTask* task : pending_tasks_) {
          task->Cancel();
        }
      },
      [&done] { done.Set(); }));
  done.Wait(rtc::Event::kForever);
}

void PendingTaskHandler::PostDelayedTask(closure_t&& closure,
                                         closure_t&& destroy,
                                         units::TimeDelta delay) {
  {
    rtc::CritScope cs(&pending_lock_);
    if (stopped_) {
      destroy();
      return;
    } else {
      std::unique_ptr<PendingTask> pending_task = rtc::MakeUnique<PendingTask>(
          std::move(closure), std::move(destroy), this);
      pending_tasks_.insert(pending_task.get());
      target_queue_->PostDelayedTask(std::move(pending_task), delay.ms());
    }
  }
}

void PendingTaskHandler::OnTaskDone(PendingTask* finished_task) {
  rtc::CritScope cs(&pending_lock_);
  pending_tasks_.erase(finished_task);
}

}  // namespace signal
}  // namespace network
}  // namespace webrtc
