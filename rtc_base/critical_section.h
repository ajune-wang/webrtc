/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CRITICAL_SECTION_H_
#define RTC_BASE_CRITICAL_SECTION_H_

#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

#if defined(WEBRTC_WIN)
#include "rtc_base/synchronization/critical_section_win.h"
#elif defined(WEBRTC_MAC) && defined(RTC_USE_UNNATIVE_MUTEX_ON_MAC)
#include "rtc_base/synchronization/critical_section_mac_unnative.h"
#elif defined(WEBRTC_POSIX)
#include "rtc_base/synchronization/critical_section_posix.h"
#else
#error Unsupported platform.
#endif

namespace rtc {

// Locking methods (Enter, TryEnter, Leave)are const to permit protecting
// members inside a const context without requiring mutable CriticalSections
// everywhere. CriticalSection is reentrant lock.
class RTC_LOCKABLE RTC_EXPORT CriticalSection {
 public:
  CriticalSection() { RTC_DCHECK(InitCheck()); }
  CriticalSection(const CriticalSection&) = delete;
  CriticalSection& operator=(const CriticalSection&) = delete;
  ~CriticalSection() = default;

  void Enter() const RTC_EXCLUSIVE_LOCK_FUNCTION() {
    impl_.Enter();
    RTC_DCHECK(CheckEnter());
  }
  bool TryEnter() const RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    if (impl_.TryEnter()) {
      RTC_DCHECK(CheckEnter());
      return true;
    }
    return false;
  }
  void Leave() const RTC_UNLOCK_FUNCTION() {
    RTC_DCHECK(CheckLeave());
    return impl_.Leave();
  }

 private:
  // Only used by RTC_DCHECKs.
  bool InitCheck();
  bool CheckEnter() const;
  bool CheckLeave() const;

  mutable webrtc::webrtc_critical_section_internal::CriticalSectionImpl impl_;
  mutable PlatformThreadRef thread_;  // Only used by RTC_DCHECKs.
  mutable int recursion_count_;       // Only used by RTC_DCHECKs.
};

// CritScope, for serializing execution through a scope.
class RTC_SCOPED_LOCKABLE CritScope {
 public:
  explicit CritScope(const CriticalSection* cs) RTC_EXCLUSIVE_LOCK_FUNCTION(cs);
  ~CritScope() RTC_UNLOCK_FUNCTION();

 private:
  const CriticalSection* const cs_;
  RTC_DISALLOW_COPY_AND_ASSIGN(CritScope);
};

// A lock used to protect global variables. Do NOT use for other purposes.
class RTC_LOCKABLE GlobalLock {
 public:
  constexpr GlobalLock() : lock_acquired_(0) {}

  void Lock() RTC_EXCLUSIVE_LOCK_FUNCTION();
  void Unlock() RTC_UNLOCK_FUNCTION();

 private:
  volatile int lock_acquired_;
};

// GlobalLockScope, for serializing execution through a scope.
class RTC_SCOPED_LOCKABLE GlobalLockScope {
 public:
  explicit GlobalLockScope(GlobalLock* lock) RTC_EXCLUSIVE_LOCK_FUNCTION(lock);
  ~GlobalLockScope() RTC_UNLOCK_FUNCTION();

 private:
  GlobalLock* const lock_;
  RTC_DISALLOW_COPY_AND_ASSIGN(GlobalLockScope);
};

}  // namespace rtc

#endif  // RTC_BASE_CRITICAL_SECTION_H_
