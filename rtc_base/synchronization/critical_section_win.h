/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYNCHRONIZATION_CRITICAL_SECTION_WIN_H_
#define RTC_BASE_SYNCHRONIZATION_CRITICAL_SECTION_WIN_H_

// clang-format off
// clang formating would change include order.

// Include winsock2.h before including <windows.h> to maintain consistency with
// win32.h. To include win32.h directly, it must be broken out into its own
// build target.
#include <winsock2.h>
#include <windows.h>
#include <sal.h>  // must come after windows headers.
// clang-format on

namespace webrtc {
namespace webrtc_critical_section_internal {

class CriticalSectionImpl {
 public:
  CriticalSectionImpl() { InitializeCriticalSection(&crit_); }
  ~CriticalSectionImpl() { DeleteCriticalSection(&crit_); }

  void Enter() { EnterCriticalSection(&crit_); }
  bool TryEnter() { return TryEnterCriticalSection(&crit_) != FALSE; }
  void Leave() { LeaveCriticalSection(&crit_); }

 private:
  CRITICAL_SECTION crit_;
};

inline void Yield() {
  ::Sleep(0);
}

}  // namespace webrtc_critical_section_internal
}  // namespace webrtc

#endif  // RTC_BASE_SYNCHRONIZATION_CRITICAL_SECTION_WIN_H_
