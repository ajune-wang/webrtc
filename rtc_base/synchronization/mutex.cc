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

#include <time.h>

#include "rtc_base/atomic_ops.h"
#include "rtc_base/checks.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/system/unused.h"

namespace webrtc {

void GlobalMutex::Lock() {
#if !defined(WEBRTC_WIN) && \
    (!defined(WEBRTC_MAC) || RTC_USE_NATIVE_MUTEX_ON_MAC)
  const struct timespec ts_null = {0};
#endif

  while (rtc::AtomicOps::CompareAndSwap(&mutex_locked_, 0, 1)) {
#if defined(WEBRTC_WIN)
    ::Sleep(0);
#else
    nanosleep(&ts_null, nullptr);
#endif
  }
}

void GlobalMutex::Unlock() {
  int old_value = rtc::AtomicOps::CompareAndSwap(&mutex_locked_, 1, 0);
  RTC_DCHECK_EQ(1, old_value) << "Unlock called without calling Lock first";
}

GlobalMutexLock::GlobalMutexLock(GlobalMutex* mutex) : mutex_(mutex) {
  mutex_->Lock();
}

GlobalMutexLock::~GlobalMutexLock() {
  mutex_->Unlock();
}

}  // namespace webrtc
