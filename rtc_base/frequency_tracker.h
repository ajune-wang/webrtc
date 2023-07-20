/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FREQUENCY_TRACKER_H_
#define RTC_BASE_FREQUENCY_TRACKER_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {
// Class to estimate frequency (e.g. frame rate) over running window.
// Timestamps used in Update(), Rate() and SetWindowSize() must never
// decrease for two consecutive calls.
class RTC_EXPORT FrequencyTracker {
 public:
  explicit FrequencyTracker(TimeDelta window_size);

  FrequencyTracker(const FrequencyTracker&) = default;
  FrequencyTracker& operator=(const FrequencyTracker&) = delete;
  FrequencyTracker(FrequencyTracker&&) = default;
  FrequencyTracker& operator=(FrequencyTracker&&) = delete;

  ~FrequencyTracker() = default;

  // Reset instance to original state.
  void Reset() { impl_.Reset(); }

  // Update rate with a new data point, moving averaging window as needed.
  void Update(int64_t count, Timestamp now);
  void Update(Timestamp now) { Update(1, now); }

  // Note that despite this being a const method, it still updates the internal
  // state (moves averaging window), but it doesn't make any alterations that
  // are observable from the other methods, as long as supplied timestamps are
  // from a monotonic clock. Ie, it doesn't matter if this call moves the
  // window, since any subsequent call to Update or Rate would still have moved
  // the window as much or more.
  absl::optional<Frequency> Rate(Timestamp now) const;

 private:
  RateStatistics impl_;
};
}  // namespace webrtc

#endif  // RTC_BASE_FREQUENCY_TRACKER_H_
