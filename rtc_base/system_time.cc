/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// If WEBRTC_EXCLUDE_SYSTEM_TIME is set, an implementation of
// rtc::SystemTimeNanos() must be provided externally.
#ifndef WEBRTC_EXCLUDE_SYSTEM_TIME

#include <stdint.h>

#include <atomic>
#include <limits>

#if defined(WEBRTC_POSIX)
#include <sys/time.h>
#if defined(WEBRTC_MAC)
#include <mach/mach_time.h>
#endif
#endif

#if defined(WEBRTC_WIN)
// clang-format off
// clang formatting would put <windows.h> last,
// which leads to compilation failure.
#include <windows.h>
#include <mmsystem.h>
#include <sys/timeb.h>
// clang-format on
#endif

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/system_time.h"
#include "rtc_base/time_utils.h"

namespace rtc {

int64_t SystemTimeNanos() {
  int64_t ticks;
#if defined(WEBRTC_MAC)
  static mach_timebase_info_data_t timebase;
  if (timebase.denom == 0) {
    // Get the timebase if this is the first time we run.
    // Recommended by Apple's QA1398.
    if (mach_timebase_info(&timebase) != KERN_SUCCESS) {
      RTC_DCHECK_NOTREACHED();
    }
  }
  // Use timebase to convert absolute time tick units into nanoseconds.
  const auto mul = [](uint64_t a, uint32_t b) -> int64_t {
    RTC_DCHECK_NE(b, 0);
    RTC_DCHECK_LE(a, std::numeric_limits<int64_t>::max() / b)
        << "The multiplication " << a << " * " << b << " overflows";
    return rtc::dchecked_cast<int64_t>(a * b);
  };
  ticks = mul(mach_absolute_time(), timebase.numer) / timebase.denom;
#elif defined(WEBRTC_POSIX)
  struct timespec ts;
  // TODO(deadbeef): Do we need to handle the case when CLOCK_MONOTONIC is not
  // supported?
  clock_gettime(CLOCK_MONOTONIC, &ts);
  ticks = kNumNanosecsPerSec * static_cast<int64_t>(ts.tv_sec) +
          static_cast<int64_t>(ts.tv_nsec);
#elif defined(WINUWP)
  ticks = WinUwpSystemTimeNanos();
#elif defined(WEBRTC_WIN)
  // Code based on Chromium's base/time/time_win.cc
  // The implementation calls timeGetTime(). The problem is that
  // this function returns a 32-bit time, which wraps around roughly every
  // 49 days. The code below tracks the number of "rollovers" that have
  // occurred, in a tread safe manner.

  // A structure holding the 8 most significant bits of the last
  // timestamp, followed by a 24 bit rollover counter. By encoding
  // both in the same atomic variable, we ensure that both are always
  // updated together.
  static std::atomic<uint32_t> last_8_and_rollover_count = 0;

  uint32_t rollover_count;
  DWORD now_ms;  // DWORD is always unsigned 32 bits.

  while (true) {
    // Fetch (8 most significant bits of) last time, and the rollover count.
    uint32_t original =
        last_8_and_rollover_count.load(std::memory_order_acquire);
    uint8_t last_8 = static_cast<uint8_t>(original >> 24);
    rollover_count = original & 0x00FFFFFF;

    // Get curent time and update rollover_count if it has wrapped around.
    now_ms = timeGetTime();
    uint8_t now_8 = static_cast<uint8_t>(now_ms >> 24);
    if (now_8 < last_8)
      ++rollover_count;

    // Update state with 8 most significant bits of current time,
    // followed by 24 bits of rollover counter.
    uint32_t new_state =
        (static_cast<uint32_t>(now_8) << 24) + (rollover_count & 0x00FFFFFF);

    // If the state hasn't changed, exit the loop. (Likely. The top 8 bits of a
    // 32-bit millisecond timestamp only changes once every 4.6 hours, and
    // rollover only occurs once every 49 days.)
    if (new_state == original)
      break;

    // Save the new state if no other thread has changed the original value.
    uint32_t check = last_8_and_rollover_count.compare_exchange_strong(
        original, new_state, std::memory_order_release);
    if (check == original)
      break;

    // Another thread has done something in between so retry from the top.
  }

  ticks = static_cast<int64_t>(now_ms);
  ticks += static_cast<int64_t>(rollover_count) << 32;
  ticks *= kNumNanosecsPerMillisec;

#else
#error Unsupported platform.
#endif
  return ticks;
}

}  // namespace rtc
#endif  // WEBRTC_EXCLUDE_SYSTEM_TIME
