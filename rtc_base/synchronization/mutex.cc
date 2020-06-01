/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/synchronization/mutex.h"

#include <thread>  // For std::this_thread::yield() only.

#include "rtc_base/checks.h"

namespace webrtc {

#if !defined(WEBRTC_ABSL_MUTEX)
void GlobalMutex::Lock() {
  int old_mutex_locked = 0;
  while (!mutex_locked_.compare_exchange_strong(old_mutex_locked, 1)) {
    std::this_thread::yield();
    old_mutex_locked = 0;
  }
}

void GlobalMutex::Unlock() {
  int old_mutex_locked = 1;
  bool success = mutex_locked_.compare_exchange_strong(old_mutex_locked, 0);
  RTC_DCHECK_EQ(1, success) << "Unlock called without calling Lock first";
}

GlobalMutexLock::GlobalMutexLock(GlobalMutex* mutex) : mutex_(mutex) {
  mutex_->Lock();
}

GlobalMutexLock::~GlobalMutexLock() {
  mutex_->Unlock();
}
#endif  // #if !defined(WEBRTC_ABSL_MUTEX)

}  // namespace webrtc
