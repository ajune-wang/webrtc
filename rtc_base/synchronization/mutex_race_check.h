/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYNCHRONIZATION_MUTEX_RACE_CHECK_H_
#define RTC_BASE_SYNCHRONIZATION_MUTEX_RACE_CHECK_H_

#include <atomic>

#include "rtc_base/checks.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// This implementation class is useful when a consuming project can guarantee
// that all WebRTC invocation is happening serially.
class RTC_LOCKABLE MutexImpl final {
 public:
  MutexImpl() = default;
  MutexImpl(const MutexImpl&) = delete;
  MutexImpl& operator=(const MutexImpl&) = delete;

  void Lock() RTC_EXCLUSIVE_LOCK_FUNCTION() {
    bool was_free = free_.exchange(false, std::memory_order_acquire);
    RTC_CHECK(was_free);
  }
  RTC_WARN_UNUSED_RESULT bool TryLock() RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return free_.exchange(false, std::memory_order_acquire);
  }
  void Unlock() RTC_UNLOCK_FUNCTION() {
    free_.store(true, std::memory_order_release);
  }

 private:
  // A word on the ordering. Release-acquire ordering is used; in the Lock
  // methods we're guaranteeing no other threads observes reads and writes after
  // the Lock happening before the Lock (acquire ordering). In the Unlock method
  // we're guaranteeing no other thread observes reads and writes happening
  // before the Unlock (release ordering).
  std::atomic<bool> free_{true};
};

}  // namespace webrtc

#endif  // RTC_BASE_SYNCHRONIZATION_MUTEX_RACE_CHECK_H_
