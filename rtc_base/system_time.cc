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

  // A structure holding the most significant bits of "last seen" and a
  // "rollover" counter.
  union LastTimeAndRolloversState {
    // The state as a single 32-bit opaque value.
    std::atomic<int32_t> as_opaque_32{0};

    // The state as usable values.
    struct {
      // The top 8-bits of the "last" time. This is enough to check for
      // rollovers and the small bit-size means fewer CompareAndSwap operations
      // to store changes in state, which in turn makes for fewer retries.
      uint8_t last_8;
      // A count of the number of detected rollovers. Using this as bits 47-32
      // of the upper half of a 64-bit value results in a 48-bit tick counter.
      // This extends the total rollover period from about 49 days to about 8800
      // years while still allowing it to be stored with last_8 in a single
      // 32-bit value.
      uint16_t rollovers;
    } as_values;
  };
  static std::atomic<int32_t> last_time_and_rollovers = 0;
  static_assert(
      sizeof(LastTimeAndRolloversState) <= sizeof(last_time_and_rollovers),
      "LastTimeAndRolloversState does not fit in a single atomic word");

  LastTimeAndRolloversState state;
  DWORD now_ms;  // DWORD is always unsigned 32 bits.

  while (true) {
    // Fetch the "now" and "last" tick values, updating "last" with "now" and
    // incrementing the "rollovers" counter if the tick-value has wrapped back
    // around. Atomic operations ensure that both "last" and "rollovers" are
    // always updated together.
    int32_t original = last_time_and_rollovers.load(std::memory_order_acquire);
    state.as_opaque_32 = original;
    now_ms = timeGetTime();
    uint8_t now_8 = static_cast<uint8_t>(now_ms >> 24);
    if (now_8 < state.as_values.last_8)
      ++state.as_values.rollovers;
    state.as_values.last_8 = now_8;

    // If the state hasn't changed, exit the loop.
    if (state.as_opaque_32 == original)
      break;

    // Save the changed state. If the existing value is unchanged from the
    // original, exit the loop.
    int32_t check = last_time_and_rollovers.compare_exchange_strong(
        original, state.as_opaque_32, std::memory_order_release);
    if (check == original)
      break;

    // Another thread has done something in between so retry from the top.
  }

  ticks = static_cast<int64_t>(now_ms);
  ticks += static_cast<int64_t>(state.as_values.rollovers) << 32;
  ticks *= kNumNanosecsPerMillisec;

#else
#error Unsupported platform.
#endif
  return ticks;
}

}  // namespace rtc
#endif  // WEBRTC_EXCLUDE_SYSTEM_TIME
