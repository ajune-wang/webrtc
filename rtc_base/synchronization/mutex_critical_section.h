/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYNCHRONIZATION_MUTEX_CRITICAL_SECTION_H_
#define RTC_BASE_SYNCHRONIZATION_MUTEX_CRITICAL_SECTION_H_

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

#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTC_LOCKABLE MutexImpl {
 public:
  MutexImpl() { InitializeCriticalSection(&crit_); }
  ~MutexImpl() { DeleteCriticalSection(&crit_); }

  void Lock() RTC_EXCLUSIVE_LOCK_FUNCTION() { EnterCriticalSection(&crit_); }
  RTC_WARN_UNUSED_RESULT bool TryLock() RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return TryEnterCriticalSection(&crit_) != FALSE;
  }
  void Unlock() RTC_UNLOCK_FUNCTION() { LeaveCriticalSection(&crit_); }

 private:
  CRITICAL_SECTION crit_;
};

}  // namespace webrtc

#endif  // #if defined(WEBRTC_WIN)
#endif  // RTC_BASE_SYNCHRONIZATION_MUTEX_CRITICAL_SECTION_H_
