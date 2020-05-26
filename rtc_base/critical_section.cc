/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/critical_section.h"

#include "rtc_base/atomic_ops.h"
#include "rtc_base/checks.h"
#include "rtc_base/platform_thread_types.h"

namespace rtc {

bool CriticalSection::InitCheck() {
  thread_ = 0;
  recursion_count_ = 0;
  return true;
}

bool CriticalSection::CheckEnter() const {
  if (!recursion_count_) {
    RTC_DCHECK(!thread_);
    thread_ = CurrentThreadRef();
  } else {
    RTC_DCHECK(IsThreadRefEqual(thread_, CurrentThreadRef()));
  }
  ++recursion_count_;
  return true;
}

bool CriticalSection::CheckLeave() const {
  --recursion_count_;
  RTC_DCHECK_GE(recursion_count_, 0);
  if (recursion_count_ == 0)
    thread_ = 0;
  return true;
}

CritScope::CritScope(const CriticalSection* cs) : cs_(cs) {
  cs_->Enter();
}
CritScope::~CritScope() {
  cs_->Leave();
}

void GlobalLock::Lock() {
  while (AtomicOps::CompareAndSwap(&lock_acquired_, 0, 1)) {
    webrtc::webrtc_critical_section_internal::Yield();
  }
}

void GlobalLock::Unlock() {
  int old_value = AtomicOps::CompareAndSwap(&lock_acquired_, 1, 0);
  RTC_DCHECK_EQ(1, old_value) << "Unlock called without calling Lock first";
}

GlobalLockScope::GlobalLockScope(GlobalLock* lock) : lock_(lock) {
  lock_->Lock();
}

GlobalLockScope::~GlobalLockScope() {
  lock_->Unlock();
}

}  // namespace rtc
