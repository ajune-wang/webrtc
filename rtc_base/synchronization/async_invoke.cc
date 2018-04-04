/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/synchronization/async_invoke.h"

#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/task_queue.h"

namespace rtc {

InvokeWaiter::InvokeWaiter() : done_(false, false) {}

InvokeWaiter::~InvokeWaiter() {
  RTC_DCHECK_EQ(blocker_count_, 0);
}

InvokeDoneBlocker InvokeWaiter::CreateBlocker() {
  return InvokeDoneBlocker(this);
}

void InvokeWaiter::Wait() {
  RTC_DCHECK(!TaskQueue::Current());
  if (rtc::AtomicOps::AcquireLoad(&blocker_count_) > 0) {
    bool success = done_.Wait(Event::kForever);
    RTC_DCHECK(success);
  } else {
    RTC_LOG(LS_WARNING) << "No blockers to wait for";
  }
}

AutoWaiter::~AutoWaiter() {
  Wait();
}

void InvokeWaiter::AddBlock() {
  if (rtc::AtomicOps::Increment(&blocker_count_) == 1) {
    done_.Reset();
  }
}

void InvokeWaiter::DecBlock() {
  if (rtc::AtomicOps::Decrement(&blocker_count_) == 0) {
    done_.Set();
  }
}

InvokeDoneBlocker::InvokeDoneBlocker(InvokeWaiter* target) : target_(target) {
  RTC_LOG(LS_VERBOSE) << "Creating " << this;
  if (target_)
    target_->AddBlock();
}

InvokeDoneBlocker::InvokeDoneBlocker() : target_(nullptr) {}

InvokeDoneBlocker::InvokeDoneBlocker(InvokeDoneBlocker&& other)
    : target_(other.target_) {
  RTC_LOG(LS_VERBOSE) << "Moving to " << this << " from " << &other;
  other.target_ = nullptr;
}

InvokeDoneBlocker::InvokeDoneBlocker(const InvokeDoneBlocker& other)
    : target_(other.target_) {
  RTC_LOG(LS_VERBOSE) << "Copying to " << this << " from " << &other;
  if (target_)
    target_->AddBlock();
}

void InvokeDoneBlocker::operator=(const InvokeDoneBlocker& other) {
  target_ = other.target_;
  RTC_LOG(LS_VERBOSE) << "Assign to " << this << " from " << &other;
  if (target_)
    target_->AddBlock();
}

InvokeDoneBlocker::~InvokeDoneBlocker() {
  RTC_LOG(LS_VERBOSE) << "Destroying " << this;
  if (target_)
    target_->DecBlock();
}

InvokeDoneBlocker InvokeDoneBlocker::NonBlocking() {
  return InvokeDoneBlocker();
}
bool InvokeDoneBlocker::IsBlocking() const {
  return target_ != nullptr;
}

}  // namespace rtc
