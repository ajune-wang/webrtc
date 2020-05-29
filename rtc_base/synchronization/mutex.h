/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYNCHRONIZATION_MUTEX_H_
#define RTC_BASE_SYNCHRONIZATION_MUTEX_H_

#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/system/unused.h"
#include "rtc_base/thread_annotations.h"

#if defined(WEBRTC_STD_MUTEX)
#include "rtc_base/synchronization/mutex_std.h"
#elif defined(WEBRTC_ABSL_MUTEX)
#include "rtc_base/synchronization/mutex_abseil.h"
#elif defined(WEBRTC_WIN)
#include "rtc_base/synchronization/mutex_critical_section.h"
#elif defined(WEBRTC_POSIX)
#include "rtc_base/synchronization/mutex_pthread.h"
#else
#error Unsupported platform.
#endif

namespace webrtc {

// The Mutex provides concurrent access protection and aims to follow Abseil
// semantics (i.e. non-reentrant etc).
// The Lock/TryLock/Unlock methods are const for backwards compatibility
// with rtc::CriticalSection.
// TODO(bugs.webrtc.org/11567) remove this requirement - let clients use
// the mutable keyword instead.
class RTC_LOCKABLE Mutex {
 public:
  void Lock() const RTC_EXCLUSIVE_LOCK_FUNCTION() { impl_.Lock(); }
  RTC_WARN_UNUSED_RESULT bool TryLock() const
      RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return impl_.TryLock();
  }
  void Unlock() const RTC_UNLOCK_FUNCTION() { impl_.Unlock(); }

 private:
  mutable webrtc_impl::MutexImpl impl_;
};

// MutexLock, for serializing execution through a scope.
class RTC_SCOPED_LOCKABLE MutexLock {
 public:
  // TODO(bugs.webrtc.org/11567) remove const and let clients use
  // the mutable keyword instead.
  explicit MutexLock(const Mutex* mu) RTC_EXCLUSIVE_LOCK_FUNCTION(mu)
      : mu_(mu) {
    mu->Lock();
  }
  ~MutexLock() RTC_UNLOCK_FUNCTION() { mu_->Unlock(); }

 private:
  const Mutex* mu_;
};

// A mutex used to protect global variables. Do NOT use for other purposes.
class RTC_LOCKABLE GlobalMutex {
 public:
  constexpr GlobalMutex() : mutex_locked_(0) {}

  void Lock() RTC_EXCLUSIVE_LOCK_FUNCTION();
  void Unlock() RTC_UNLOCK_FUNCTION();

 private:
  volatile int mutex_locked_;
};

// GlobalMutexLock, for serializing execution through a scope.
class RTC_SCOPED_LOCKABLE GlobalMutexLock {
 public:
  explicit GlobalMutexLock(GlobalMutex* mu) RTC_EXCLUSIVE_LOCK_FUNCTION();
  ~GlobalMutexLock() RTC_UNLOCK_FUNCTION();

 private:
  GlobalMutex* mu_;
};

}  // namespace webrtc

#endif  // RTC_BASE_SYNCHRONIZATION_MUTEX_H_
