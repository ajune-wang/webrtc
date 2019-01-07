/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/stats_counter.h"

#include <algorithm>
#include <cmath>

#include "rtc_base/checks.h"

namespace webrtc {

BasicStatsCounter::BasicStatsCounter() = default;
BasicStatsCounter::~BasicStatsCounter() = default;

void BasicStatsCounter::AddSample(double value) {
  samples_.push_back(value);
  sorted_ = false;
  if (value > max_) {
    max_ = value;
  }
  if (value < min_) {
    min_ = value;
  }
  sum_ += value;
}

bool BasicStatsCounter::HasValues() const {
  return !samples_.empty();
}

double BasicStatsCounter::GetMin() const {
  RTC_CHECK(samples_.size() > 0) << "Stats is empty";
  return min_;
}

double BasicStatsCounter::GetMax() const {
  RTC_CHECK(samples_.size() > 0) << "Stats is empty";
  return max_;
}

double BasicStatsCounter::GetAverage() const {
  RTC_CHECK(samples_.size() > 0) << "Stats is empty";
  return sum_ / samples_.size();
}

double BasicStatsCounter::GetPercentile(double percentile) {
  RTC_CHECK(samples_.size() > 0) << "Stats is empty";
  RTC_CHECK(0 < percentile && percentile <= 1);
  if (!sorted_) {
    std::sort(samples_.begin(), samples_.end());
    sorted_ = true;
  }
  double raw_rank = percentile * samples_.size();
  double int_part;
  double fract_part = std::modf(raw_rank, &int_part);
  double a = int_part == 0 ? samples_[0] : samples_[int_part - 1];
  double b = samples_[int_part];
  return a * (1 - fract_part) + b * fract_part;
}

}  // namespace webrtc
