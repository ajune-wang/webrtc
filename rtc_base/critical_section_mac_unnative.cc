/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/critical_section_mac_unnative.h"

#include "rtc_base/atomic_ops.h"
#include "rtc_base/checks.h"
#include "rtc_base/platform_thread_types.h"

namespace webrtc {
namespace webrtc_critical_section_internal {

void CriticalSectionImpl::Enter() {
  int spin = 3000;
  PlatformThreadRef self = CurrentThreadRef();
  bool have_lock = false;
  do {
    // Instead of calling TryEnter() in this loop, we do two interlocked
    // operations, first a read-only one in order to avoid affecting the lock
    // cache-line while spinning, in case another thread is using the lock.
    if (!IsThreadRefEqual(owning_thread_, self)) {
      if (AtomicOps::AcquireLoad(&lock_queue_) == 0) {
        if (AtomicOps::CompareAndSwap(&lock_queue_, 0, 1) == 0) {
          have_lock = true;
          break;
        }
      }
    } else {
      AtomicOps::Increment(&lock_queue_);
      have_lock = true;
      break;
    }

    sched_yield();
  } while (--spin);

  if (!have_lock && AtomicOps::Increment(&lock_queue_) > 1) {
    // Owning thread cannot be the current thread since TryEnter() would
    // have succeeded.
    RTC_DCHECK(!IsThreadRefEqual(owning_thread_, self));
    // Wait for the lock to become available.
    dispatch_semaphore_wait(semaphore_, DISPATCH_TIME_FOREVER);
    RTC_DCHECK(owning_thread_ == 0);
    RTC_DCHECK(!recursion_);
  }

  owning_thread_ = self;
  ++recursion_;
}

bool CriticalSectionImpl::TryEnter() {
  if (!IsThreadRefEqual(owning_thread_, CurrentThreadRef())) {
    if (AtomicOps::CompareAndSwap(&lock_queue_, 0, 1) != 0)
      return false;
    owning_thread_ = CurrentThreadRef();
    RTC_DCHECK(!recursion_);
  } else {
    AtomicOps::Increment(&lock_queue_);
  }
  ++recursion_;
  return true;
}

void CriticalSectionImpl::Leave() const {
  RTC_DCHECK(IsThreadRefEqual(owning_thread_, CurrentThreadRef()));
  RTC_DCHECK_GE(recursion_, 0);
  --recursion_;
  if (!recursion_)
    owning_thread_ = 0;

  if (AtomicOps::Decrement(&lock_queue_) > 0 && !recursion_)
    dispatch_semaphore_signal(semaphore_);
}

}  // namespace webrtc_critical_section_internal
}  // namespace webrtc
