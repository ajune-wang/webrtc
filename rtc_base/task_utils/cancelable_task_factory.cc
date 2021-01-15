/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/cancelable_task_factory.h"
#include "rtc_base/checks.h"

namespace webrtc {

void CancelableTaskFactory::ControlBlock::AddRef() {
  ref_count_.IncRef();
}

void CancelableTaskFactory::ControlBlock::Release() {
  if (ref_count_.DecRef() == rtc::RefCountReleaseStatus::kDroppedLastRef) {
    delete this;
  }
}

void CancelableTaskFactory::ControlBlock::CancelAll() {
  {
    MutexLock lock(&mu_);
    canceled_ = true;
    if (num_running_ == 0) {
      return;
    }
  }
  // Some tasks were running, thus need to wait until they are done.
  canceling_.Wait(rtc::Event::kForever);
}

bool CancelableTaskFactory::ControlBlock::StartTask() {
  MutexLock lock(&mu_);
  if (canceled_) {
    return false;
  }
  ++num_running_;
  return true;
}

void CancelableTaskFactory::ControlBlock::CompleteTask() {
  {
    MutexLock lock(&mu_);
    RTC_DCHECK_GT(num_running_, 0);
    --num_running_;
    if (!canceled_ || num_running_ > 0) {
      return;
    }
  }
  // CancelAll was called while a task was running and this was the last
  // task running. Unblock the CancelAll function.
  canceling_.Set();
}

CancelableTaskFactory::CancelableTaskFactory() : controll_(new ControlBlock) {}

}  // namespace webrtc
