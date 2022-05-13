/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/rate_limiter.h"

#include <limits>

#include "absl/types/optional.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

RateLimiter::RateLimiter(Clock* clock, int64_t max_window_ms)
    : RateLimiter(clock, TimeDelta::Millis(max_window_ms)) {}

RateLimiter::RateLimiter(Clock* clock, TimeDelta max_window)
    : clock_(clock),
      current_rate_(max_window.ms(), RateStatistics::kBpsScale),
      window_size_(max_window),
      max_rate_(DataRate::Infinity()) {}

bool RateLimiter::TryUseRate(size_t packet_size_bytes) {
  return TryUseRate(DataSize::Bytes(packet_size_bytes));
}

bool RateLimiter::TryUseRate(DataSize packet_size) {
  MutexLock lock(&lock_);
  Timestamp now = clock_->CurrentTime();
  absl::optional<int64_t> current_rate_bps = current_rate_.Rate(now.ms());
  if (current_rate_bps) {
    // If there is a current rate, check if adding bytes would cause maximum
    // bitrate target to be exceeded. If there is NOT a valid current rate,
    // allow allocating rate even if target is exceeded. This prevents
    // problems
    // at very low rates, where for instance retransmissions would never be
    // allowed due to too high bitrate caused by a single packet.
    DataRate current_rate = DataRate::BitsPerSec(*current_rate_bps);
    DataRate bitrate_addition = packet_size / window_size_;
    if (current_rate + bitrate_addition > max_rate_)
      return false;
  }

  current_rate_.Update(packet_size.bytes(), now.ms());
  return true;
}

void RateLimiter::SetMaxRate(uint32_t max_rate_bps) {
  SetMaxRate(DataRate::BitsPerSec(max_rate_bps));
}

void RateLimiter::SetMaxRate(DataRate max_rate) {
  MutexLock lock(&lock_);
  max_rate_ = max_rate;
}

bool RateLimiter::SetWindowSize(int64_t window_size_ms) {
  return SetWindowSize(TimeDelta::Millis(window_size_ms));
}

bool RateLimiter::SetWindowSize(TimeDelta window_size) {
  MutexLock lock(&lock_);
  window_size_ = window_size;
  return current_rate_.SetWindowSize(window_size.ms(),
                                     clock_->TimeInMilliseconds());
}

}  // namespace webrtc
