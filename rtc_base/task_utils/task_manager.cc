/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/task_manager.h"
#include "rtc_base/checks.h"

namespace webrtc {

void StoppableTaskFactory::ControlBlock::AddRef() {
  ref_count_.IncRef();
}

void StoppableTaskFactory::ControlBlock::Release() {
  if (ref_count_.DecRef() == rtc::RefCountReleaseStatus::kDroppedLastRef) {
    delete this;
  }
}

void StoppableTaskFactory::ControlBlock::Stop() {
  int running;
  {
    rtc::CritScope lock(&cs_);
    RTC_DCHECK(!stopped_);
    stopped_ = true;
    running = running_;
  }
  if (running > 0) {
    stopping_.Wait(rtc::Event::kForever);
  }
}

bool StoppableTaskFactory::ControlBlock::StartTask() {
  rtc::CritScope lock(&cs_);
  if (stopped_)
    return false;
  ++running_;
  return true;
}

void StoppableTaskFactory::ControlBlock::CompletedTask() {
  bool notify_stop_function;
  {
    rtc::CritScope lock(&cs_);
    RTC_DCHECK_GT(running_, 0);
    bool last_running = --running_ == 0;
    notify_stop_function = stopped_ && last_running;
  }
  if (notify_stop_function) {
    // Was stopped while current task was running, Manager is waiting.
    stopping_.Set();
  }
}

StoppableTaskFactory::StoppableTaskFactory() : controll_(new ControlBlock) {}
StoppableTaskFactory::~StoppableTaskFactory() = default;

}  // namespace webrtc
