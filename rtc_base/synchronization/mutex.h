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

#include "absl/base/const_init.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/system/unused.h"
#include "rtc_base/thread_annotations.h"

#if defined(WEBRTC_ABSL_MUTEX)
#include "rtc_base/synchronization/mutex_abseil.h"
#elif defined(WEBRTC_WIN)
#include "rtc_base/synchronization/mutex_critical_section.h"
#elif defined(WEBRTC_POSIX)
#include "rtc_base/synchronization/mutex_pthread.h"
#else
#error Unsupported platform.
#endif

namespace webrtc {

// The Mutex guarantees exclusive access and aims to follow Abseil semantics
// (i.e. non-reentrant etc). The Lock/TryLock/Unlock methods are const for
// backwards compatibility with rtc::CriticalSection.
// TODO(bugs.webrtc.org/11567) remove this requirement - let clients use
// the mutable keyword instead.
class RTC_LOCKABLE Mutex final {
 public:
  void Lock() const RTC_EXCLUSIVE_LOCK_FUNCTION() { impl_.Lock(); }
  RTC_WARN_UNUSED_RESULT bool TryLock() const
      RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return impl_.TryLock();
  }
  void Unlock() const RTC_UNLOCK_FUNCTION() { impl_.Unlock(); }

 private:
  mutable MutexImpl impl_;
};

// MutexLock, for serializing execution through a scope.
class RTC_SCOPED_LOCKABLE MutexLock final {
 public:
  // TODO(bugs.webrtc.org/11567) remove const and let clients use
  // the mutable keyword instead.
  explicit MutexLock(const Mutex* mutex) RTC_EXCLUSIVE_LOCK_FUNCTION(mutex)
      : mutex_(mutex) {
    mutex->Lock();
  }
  ~MutexLock() RTC_UNLOCK_FUNCTION() { mutex_->Unlock(); }

 private:
  const Mutex* mutex_;
};

// A mutex used to protect global variables. Do NOT use for other purposes.
#if defined(WEBRTC_ABSL_MUTEX)
using GlobalMutex = absl::Mutex;
using GlobalMutexLock = absl::MutexLock;
#else
class RTC_LOCKABLE GlobalMutex final {
 public:
  constexpr explicit GlobalMutex(absl::ConstInitType /*unused*/)
      : mutex_locked_(0) {}

  void Lock() RTC_EXCLUSIVE_LOCK_FUNCTION();
  void Unlock() RTC_UNLOCK_FUNCTION();

 private:
  volatile int mutex_locked_;
};

// GlobalMutexLock, for serializing execution through a scope.
class RTC_SCOPED_LOCKABLE GlobalMutexLock final {
 public:
  explicit GlobalMutexLock(GlobalMutex* mutex) RTC_EXCLUSIVE_LOCK_FUNCTION();
  ~GlobalMutexLock() RTC_UNLOCK_FUNCTION();

 private:
  GlobalMutex* mutex_;
};
#endif  // if defined(WEBRTC_ABSL_MUTEX)

}  // namespace webrtc

#endif  // RTC_BASE_SYNCHRONIZATION_MUTEX_H_
