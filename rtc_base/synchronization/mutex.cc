/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
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

// TODO(tommi): Split this file up to per-platform implementation files.

#if RTC_DCHECK_IS_ON
#define RTC_CS_DEBUG_CODE(x) x
#else  // !RTC_DCHECK_IS_ON
#define RTC_CS_DEBUG_CODE(x)
#endif  // !RTC_DCHECK_IS_ON

namespace webrtc {

Mutex::Mutex() {
#if defined(WEBRTC_STD_MUTEX)
#elif defined(WEBRTC_ABSL_MUTEX)
#elif defined(WEBRTC_WIN)
  InitializeCriticalSection(&crit_);
#elif defined(WEBRTC_POSIX)
  /*pthread_mutexattr_t mutex_attribute;
  pthread_mutexattr_init(&mutex_attribute);
#if defined(WEBRTC_MAC)
  pthread_mutexattr_setpolicy_np(&mutex_attribute,
                                 _PTHREAD_MUTEX_POLICY_FAIRSHARE);
#endif
  pthread_mutex_init(&mutex_, &mutex_attribute);
  pthread_mutexattr_destroy(&mutex_attribute);*/
#else
#error Unsupported platform.
#endif
}

Mutex::~Mutex() {
#if defined(WEBRTC_STD_MUTEX)
#elif defined(WEBRTC_ABSL_MUTEX)
#elif defined(WEBRTC_WIN)
  DeleteCriticalSection(&crit_);
#elif defined(WEBRTC_POSIX)
  // pthread_mutex_destroy(&mutex_);
#else
#error Unsupported platform.
#endif
}

void Mutex::Lock() const RTC_EXCLUSIVE_LOCK_FUNCTION() {
#if defined(WEBRTC_STD_MUTEX)
  mutex_.lock();
#elif defined(WEBRTC_ABSL_MUTEX)
  mutex_.Lock();
#elif defined(WEBRTC_WIN)
  EnterCriticalSection(&crit_);
#elif defined(WEBRTC_POSIX)
  pthread_mutex_lock(&mutex_);
#else
#error Unsupported platform.
#endif
}

bool Mutex::TryLock() const RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
#if defined(WEBRTC_STD_MUTEX)
  return mutex_.try_lock();
#elif defined(WEBRTC_ABSL_MUTEX)
  return mutex_.TryLock();
#elif defined(WEBRTC_WIN)
  return TryEnterCriticalSection(&crit_) != FALSE;
#elif defined(WEBRTC_POSIX)
  return pthread_mutex_trylock(&mutex_) == 0;
#else
#error Unsupported platform.
#endif
}

void Mutex::Unlock() const RTC_UNLOCK_FUNCTION() {
#if defined(WEBRTC_STD_MUTEX)
  mutex_.unlock();
#elif defined(WEBRTC_ABSL_MUTEX)
  mutex_.Unlock();
#elif defined(WEBRTC_WIN)
  LeaveCriticalSection(&crit_);
#elif defined(WEBRTC_POSIX)
  pthread_mutex_unlock(&mutex_);
#else
#error Unsupported platform.
#endif
}

MutexLock::MutexLock(const Mutex* mu) : mu_(mu) {
  mu_->Lock();
}
MutexLock::~MutexLock() {
  mu_->Unlock();
}

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

GlobalMutexLock::GlobalMutexLock(GlobalMutex* mu) : mu_(mu) {
  mu_->Lock();
}

GlobalMutexLock::~GlobalMutexLock() {
  mu_->Unlock();
}

}  // namespace webrtc
