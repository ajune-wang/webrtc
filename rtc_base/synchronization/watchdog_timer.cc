/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/synchronization/watchdog_timer.h"

#include <set>

#include "absl/base/attributes.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/logging.h"

#ifdef WEBRTC_ANDROID
#include "sdk/android/native_api/stacktrace/stacktrace.h"
#endif

namespace webrtc {

namespace {

// The set of watchdog timers, and the lock that protects it. We create the set
// on first use, and never destroy it.
ABSL_CONST_INIT rtc::GlobalLockPod g_watchdog_lock = {};
ABSL_CONST_INIT std::set<WatchdogTimer*>* g_watchdogs = nullptr;

}  // namespace

WatchdogTimer::WatchdogTimer(rtc::Location location) : created_here_(location) {
  rtc::GlobalLockScope gls(&g_watchdog_lock);

  // Relaxed memory order is sufficient here, since we only need to sequence the
  // values of this one variable, and not any other parts of memory.
  needs_poking_.store(false, std::memory_order_relaxed);

#ifdef WEBRTC_ANDROID
  thread_id_.store(-1, std::memory_order_relaxed);
#endif

  if (g_watchdogs == nullptr) {
    g_watchdogs = new std::set<WatchdogTimer*>();
  }
  const auto result = g_watchdogs->insert(this);
  RTC_DCHECK(result.second);  // Insertion succeeded without collisions.
}

WatchdogTimer::~WatchdogTimer() {
  rtc::GlobalLockScope gls(&g_watchdog_lock);
  RTC_DCHECK(g_watchdogs != nullptr);
  int num_erased = g_watchdogs->erase(this);
  RTC_DCHECK_EQ(num_erased, 1);
}

void WatchdogTimer::CheckAll() {
  rtc::GlobalLockScope gls(&g_watchdog_lock);
  if (g_watchdogs == nullptr) {
    return;
  }
  int num_problems = 0;
  for (WatchdogTimer* const wd : *g_watchdogs) {
    // Atomically retrieve the old value and set the value to true. (Relaxed
    // memory order is sufficient here, since we only need to sequence the
    // values of this one variable, and not any other parts of memory.)
    const bool needed_poking =
        wd->needs_poking_.exchange(true, std::memory_order_relaxed);
    if (needed_poking) {
      // No one has poked this watchdog since the last time we checked it.
      ++num_problems;
      RTC_LOG(LS_ERROR) << "Timeout for webrtc::WatchdogTimer created at "
                        << wd->created_here_.ToString();
#ifdef WEBRTC_ANDROID
      const int thread_id = wd->thread_id_.load(std::memory_order_relaxed);
      if (thread_id != -1) {
        RTC_LOG(LS_ERROR) << "(no stack trace available)";
      } else {
        RTC_LOG(LS_ERROR) << StackTraceToString(GetStackTrace(thread_id));
      }
#endif
    }
  }
  if (num_problems == 0) {
    RTC_LOG(LS_INFO)
        << "Checked " << g_watchdogs->size()
        << " instances of webrtc::WatchdogTimer, and found no problems";
  } else {
    RTC_LOG(LS_ERROR) << "Checked " << g_watchdogs->size()
                      << " instances of webrtc::WatchdogTimer, and found "
                      << num_problems << " problems";
  }
}

}  // namespace webrtc
