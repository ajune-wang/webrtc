/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/samples_stats_counter.h"

#include <algorithm>
#include <cmath>

#include "rtc_base/checks.h"

namespace webrtc {

SamplesStatsCounter::SamplesStatsCounter() = default;
SamplesStatsCounter::~SamplesStatsCounter() = default;
SamplesStatsCounter::SamplesStatsCounter(SamplesStatsCounter&) = default;
SamplesStatsCounter::SamplesStatsCounter(SamplesStatsCounter&&) = default;

void SamplesStatsCounter::AddSample(double value) {
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

bool SamplesStatsCounter::IsEmpty() const {
  return samples_.empty();
}

double SamplesStatsCounter::GetMin() const {
  RTC_DCHECK(samples_.size() > 0) << "Stats is empty";
  return min_;
}

double SamplesStatsCounter::GetMax() const {
  RTC_DCHECK(samples_.size() > 0) << "Stats is empty";
  return max_;
}

double SamplesStatsCounter::GetAverage() const {
  RTC_DCHECK(samples_.size() > 0) << "Stats is empty";
  return sum_ / samples_.size();
}

double SamplesStatsCounter::GetPercentile(double percentile) {
  RTC_DCHECK(samples_.size() > 0) << "Stats is empty";
  RTC_CHECK(0 < percentile && percentile <= 1);
  if (!sorted_) {
    std::sort(samples_.begin(), samples_.end());
    sorted_ = true;
  }
  double raw_rank = percentile * samples_.size();
  double int_part;
  double fract_part = std::modf(raw_rank, &int_part);

  RTC_DCHECK(static_cast<size_t>(int_part) == int_part);
  RTC_DCHECK(0 <= int_part && int_part <= samples_.size());
  // fract_part can 1 in some cases because of floating point operations errors.
  RTC_DCHECK(0 <= fract_part && fract_part <= 1);
  RTC_DCHECK(int_part + fract_part == raw_rank);

  size_t rank = static_cast<size_t>(int_part);
  double a = rank == 0 ? samples_[0] : samples_[rank - 1];
  double b =
      rank == samples_.size() ? samples_[samples_.size() - 1] : samples_[rank];
  return a * (1 - fract_part) + b * fract_part;
}

}  // namespace webrtc
