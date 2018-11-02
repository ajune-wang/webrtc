/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/moving_average.h"

#include <algorithm>

#include "rtc_base/checks.h"

namespace rtc {

MovingAverage::MovingAverage(size_t window_size) : history_(window_size, 0) {}
MovingAverage::~MovingAverage() = default;

void MovingAverage::AddSample(int sample) {
  count_++;
  size_t index = count_ % history_.size();
  sum_ += sample - history_[index];
  history_[index] = sample;
}

absl::optional<int> MovingAverage::GetAverageRoundedDown() const {
  if (count_ == 0)
    return absl::nullopt;
  return static_cast<int>(GetUnroundedAverage());
}

absl::optional<int> MovingAverage::GetAverageRoundedToClosest() const {
  if (count_ == 0)
    return absl::nullopt;
  return static_cast<int>(GetUnroundedAverage() + 0.5);
}

double MovingAverage::GetUnroundedAverage() const {
  RTC_DCHECK_GT(count_, 0);
  return sum_ / static_cast<double>(size());
}

void MovingAverage::Reset() {
  count_ = 0;
  sum_ = 0;
  std::fill(history_.begin(), history_.end(), 0);
}

size_t MovingAverage::size() const {
  return std::min(count_, history_.size());
}
}  // namespace rtc
