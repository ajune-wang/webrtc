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
#include "rtc_base/thread_annotations.h"
#if defined(WEBRTC_WIN)
// clang-format off
// clang formating would change include order.

// Include winsock2.h before including <windows.h> to maintain consistency with
// win32.h. To include win32.h directly, it must be broken out into its own
// build target.
#include <winsock2.h>
#include <windows.h>
#include <sal.h>  // must come after windows headers.
// clang-format on
#endif  // defined(WEBRTC_WIN)

#if defined(WEBRTC_POSIX)
#include <pthread.h>
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
  Mutex();
  ~Mutex();

  void Lock() const RTC_EXCLUSIVE_LOCK_FUNCTION();
  bool TryLock() const RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true);
  void Unlock() const RTC_UNLOCK_FUNCTION();

 private:
#if defined(WEBRTC_ABSL_MUTEX)
  mutable absl::Mutex mutex_;
#elif defined(WEBRTC_WIN)
  mutable CRITICAL_SECTION crit_;
#elif defined(WEBRTC_POSIX)
  mutable pthread_mutex_t mutex_;
#else  // !defined(WEBRTC_WIN) && !defined(WEBRTC_POSIX)
#error Unsupported platform.
#endif
};

// MutexLock, for serializing execution through a scope.
class RTC_SCOPED_LOCKABLE MutexLock {
 public:
  explicit MutexLock(const Mutex* mu) RTC_EXCLUSIVE_LOCK_FUNCTION(mu);
  ~MutexLock() RTC_UNLOCK_FUNCTION();

 private:
  const Mutex* const mu_;
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
  explicit GlobalMutexLock(GlobalMutex* mu) RTC_EXCLUSIVE_LOCK_FUNCTION(mu_);
  ~GlobalMutexLock() RTC_UNLOCK_FUNCTION();

 private:
  GlobalMutex* const mu_;
};

}  // namespace webrtc

#endif  // RTC_BASE_SYNCHRONIZATION_MUTEX_H_
