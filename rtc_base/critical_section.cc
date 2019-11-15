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

#include <time.h>

#include <atomic>

#include "rtc_base/checks.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/system/unused.h"

// TODO(tommi): Split this file up to per-platform implementation files.

#if RTC_DCHECK_IS_ON
#define RTC_CS_DEBUG_CODE(x) x
#else  // !RTC_DCHECK_IS_ON
#define RTC_CS_DEBUG_CODE(x)
#endif  // !RTC_DCHECK_IS_ON

namespace rtc {

CriticalSection::CriticalSection() {
#if defined(WEBRTC_WIN)
  InitializeCriticalSection(&crit_);
#elif defined(WEBRTC_POSIX)
#if defined(WEBRTC_MAC) && !RTC_USE_NATIVE_MUTEX_ON_MAC
  lock_queue_.store(0);
  owning_thread_ = 0;
  recursion_ = 0;
  semaphore_ = dispatch_semaphore_create(0);
#else
  pthread_mutexattr_t mutex_attribute;
  pthread_mutexattr_init(&mutex_attribute);
  pthread_mutexattr_settype(&mutex_attribute, PTHREAD_MUTEX_RECURSIVE);
#if defined(WEBRTC_MAC)
  pthread_mutexattr_setpolicy_np(&mutex_attribute,
                                 _PTHREAD_MUTEX_POLICY_FAIRSHARE);
#endif
  pthread_mutex_init(&mutex_, &mutex_attribute);
  pthread_mutexattr_destroy(&mutex_attribute);
#endif
  RTC_CS_DEBUG_CODE(thread_ = 0);
  RTC_CS_DEBUG_CODE(recursion_count_ = 0);
  RTC_UNUSED(thread_);
  RTC_UNUSED(recursion_count_);
#else
#error Unsupported platform.
#endif
}

CriticalSection::~CriticalSection() {
#if defined(WEBRTC_WIN)
  DeleteCriticalSection(&crit_);
#elif defined(WEBRTC_POSIX)
#if defined(WEBRTC_MAC) && !RTC_USE_NATIVE_MUTEX_ON_MAC
  dispatch_release(semaphore_);
#else
  pthread_mutex_destroy(&mutex_);
#endif
#else
#error Unsupported platform.
#endif
}

void CriticalSection::Enter() const RTC_EXCLUSIVE_LOCK_FUNCTION() {
#if defined(WEBRTC_WIN)
  EnterCriticalSection(&crit_);
#elif defined(WEBRTC_POSIX)
#if defined(WEBRTC_MAC) && !RTC_USE_NATIVE_MUTEX_ON_MAC
  int spin = 3000;
  PlatformThreadRef self = CurrentThreadRef();
  bool have_lock = false;
  if (IsThreadRefEqual(owning_thread_, self)) {
    lock_queue_.fetch_add(1);
    have_lock = true;
  } else {
    do {
      // Instead of calling TryEnter() in this loop, we do two interlocked
      // operations, first a read-only one in order to avoid affecting the lock
      // cache-line while spinning, in case another thread is using the lock.
      if (lock_queue.load(std::memory_order_acquire) == 0) {
        bool expected = 0;
        if (lock_queue_.compare_exchange_weak(0, /*desired=*/1,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed)) {
          have_lock = true;
          break;
        }
      }

      sched_yield();
    } while (--spin);
  }

  if (!have_lock && lock_queue_.fetch_add(1) > 0) {
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

#else
  pthread_mutex_lock(&mutex_);
#endif

#if RTC_DCHECK_IS_ON
  if (!recursion_count_) {
    RTC_DCHECK(!thread_);
    thread_ = CurrentThreadRef();
  } else {
    RTC_DCHECK(CurrentThreadIsOwner());
  }
  ++recursion_count_;
#endif
#else
#error Unsupported platform.
#endif
}

bool CriticalSection::TryEnter() const RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
#if defined(WEBRTC_WIN)
  return TryEnterCriticalSection(&crit_) != FALSE;
#elif defined(WEBRTC_POSIX)
#if defined(WEBRTC_MAC) && !RTC_USE_NATIVE_MUTEX_ON_MAC
  if (IsThreadRefEqual(owning_thread_, CurrentThreadRef())) {
    lock_queue_.fetch_add(std::memory_order_relaxed);
  } else {
    int expected = 0;
    if (!lock_queue_.compare_exchange_strong(expected, 1,
                                             std::memory_order_acquire,
                                             std::memory_order_relaxed)) {
      return false;
    owning_thread_ = CurrentThreadRef();
    RTC_DCHECK(!recursion_);
    }
  ++recursion_;
#else
  if (pthread_mutex_trylock(&mutex_) != 0)
    return false;
#endif
#if RTC_DCHECK_IS_ON
  if (!recursion_count_) {
    RTC_DCHECK(!thread_);
    thread_ = CurrentThreadRef();
  } else {
    RTC_DCHECK(CurrentThreadIsOwner());
  }
  ++recursion_count_;
#endif
  return true;
#else
#error Unsupported platform.
#endif
}

void CriticalSection::Leave() const RTC_UNLOCK_FUNCTION() {
  RTC_DCHECK(CurrentThreadIsOwner());
#if defined(WEBRTC_WIN)
  LeaveCriticalSection(&crit_);
#elif defined(WEBRTC_POSIX)
#if RTC_DCHECK_IS_ON
  --recursion_count_;
  RTC_DCHECK(recursion_count_ >= 0);
  if (!recursion_count_)
    thread_ = 0;
#endif
#if defined(WEBRTC_MAC) && !RTC_USE_NATIVE_MUTEX_ON_MAC
  RTC_DCHECK(IsThreadRefEqual(owning_thread_, CurrentThreadRef()));
  RTC_DCHECK_GE(recursion_, 0);
  --recursion_;
  if (!recursion_)
    owning_thread_ = 0;

  if (lock_queue_.fetch_sub(1) > 1 && recursion_ == 0) {
    dispatch_semaphore_signal(semaphore_);
#else
  pthread_mutex_unlock(&mutex_);
#endif
#else
#error Unsupported platform.
#endif
}

bool CriticalSection::CurrentThreadIsOwner() const {
#if defined(WEBRTC_WIN)
  // OwningThread has type HANDLE but actually contains the Thread ID:
  // http://stackoverflow.com/questions/12675301/why-is-the-owningthread-member-of-critical-section-of-type-handle-when-it-is-de
  // Converting through size_t avoids the VS 2015 warning C4312: conversion from
  // 'type1' to 'type2' of greater size
  return crit_.OwningThread ==
         reinterpret_cast<HANDLE>(static_cast<size_t>(GetCurrentThreadId()));
#elif defined(WEBRTC_POSIX)
#if RTC_DCHECK_IS_ON
  return IsThreadRefEqual(thread_, CurrentThreadRef());
#else
  return true;
#endif  // RTC_DCHECK_IS_ON
#else
#error Unsupported platform.
#endif
}

CritScope::CritScope(const CriticalSection* cs) : cs_(cs) {
  cs_->Enter();
}
CritScope::~CritScope() {
  cs_->Leave();
}

void GlobalLock::Lock() {
#if !defined(WEBRTC_WIN) && \
    (!defined(WEBRTC_MAC) || RTC_USE_NATIVE_MUTEX_ON_MAC)
  const struct timespec ts_null = {0};
#endif

  while (true) {
    bool expected = false;
    if (lock_acquired_.compare_exchange_weak(
            expected, /*desired=*/true,
            /*success=*/std::memory_order_acquire,
            /*failure=*/std::memory_order_relaxed)) {
      // Lock acquired.
      return;
    }
#if defined(WEBRTC_WIN)
    ::Sleep(0);
#elif defined(WEBRTC_MAC) && !RTC_USE_NATIVE_MUTEX_ON_MAC
    sched_yield();
#else
    nanosleep(&ts_null, nullptr);
#endif
  }
}

void GlobalLock::Unlock() {
  bool unlocked = lock_acquired_.exchange(false, std::memory_order_release);
  RTC_DCHECK(unlocked) << "Unlock called without calling Lock first";
}

GlobalLockScope::GlobalLockScope(GlobalLock* lock) : lock_(lock) {
  lock_->Lock();
}

GlobalLockScope::~GlobalLockScope() {
  lock_->Unlock();
}

}  // namespace rtc
