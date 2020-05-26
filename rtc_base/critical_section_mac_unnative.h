/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CRITICAL_SECTION_MAC_UNNATIVE_H_
#define RTC_BASE_CRITICAL_SECTION_MAC_UNNATIVE_H_

#include <dispatch/dispatch.h>

#include "rtc_base/platform_thread_types.h"

namespace webrtc {
namespace webrtc_critical_section_internal {

class CriticalSectionImpl {
 public:
  CriticalSectionImpl() = default;
  ~CriticalSectionImpl() { dispatch_release(semaphore_); }

  void Enter();
  bool TryEnter();
  void Leave();

 private:
  // Number of times the lock has been locked + number of threads waiting.
  // TODO(tommi): We could use this number and subtract the recursion count
  // to find places where we have multiple threads contending on the same lock.
  volatile int lock_queue_ = 0;
  // |recursion_| represents the recursion count + 1 for the thread that owns
  // the lock. Only modified by the thread that owns the lock.
  int recursion_ = 0;
  // Used to signal a single waiting thread when the lock becomes available.
  dispatch_semaphore_t semaphore_ = dispatch_semaphore_create(0);
  // The thread that currently holds the lock. Required to handle recursion.
  PlatformThreadRef owning_thread_ = 0;
};

inline void Yield() {
  sched_yield();
}

}  // namespace webrtc_critical_section_internal
}  // namespace webrtc

#endif  // RTC_BASE_CRITICAL_SECTION_MAC_UNNATIVE_H_
