/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Borrowed from Chromium's src/base/threading/thread_checker.h.

#ifndef RTC_BASE_THREAD_CHECKER_H_
#define RTC_BASE_THREAD_CHECKER_H_

#include "rtc_base/deprecation.h"
#include "rtc_base/synchronization/sequence_checker.h"

#if RTC_DCHECK_IS_ON
#define RTC_THREAD_CHECKER(name) rtc::ThreadChecker name
#define RTC_DETACH_FROM_THREAD(name) (name).Detach()
#define RTC_GUARDED_BY_THREAD(name) RTC_GUARDED_BY(name)
#define RTC_PT_GUARDED_BY_THREAD(name) RTC_PT_GUARDED_BY(name)
#else  // RTC_DCHECK_IS_ON
#define RTC_THREAD_CHECKER(name) static_assert(true, "")
#define RTC_DETACH_FROM_THREAD(name)
#define RTC_GUARDED_BY_THREAD(name)
#define RTC_PT_GUARDED_BY_THREAD(name)
#endif  // RTC_DCHECK_IS_ON

namespace rtc {
class ThreadChecker : public webrtc::SequenceChecker {
 public:
  RTC_DEPRECATED bool CalledOnValidThread() const { return IsCurrent(); }
  RTC_DEPRECATED void DetachFromThread() { Detach(); }
};
}  // namespace rtc
#endif  // RTC_BASE_THREAD_CHECKER_H_
