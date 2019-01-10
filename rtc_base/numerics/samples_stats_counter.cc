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

namespace webrtc {
namespace {
constexpr double kFloatingPointError = 1e-6;
}

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

double SamplesStatsCounter::GetPercentile(double percentile) {
  RTC_DCHECK(!IsEmpty());
  RTC_CHECK_GT(percentile, 0);
  RTC_CHECK_LE(percentile, 1);
  if (!sorted_) {
    std::sort(samples_.begin(), samples_.end());
    sorted_ = true;
  }
  double raw_rank = percentile * samples_.size();

  // rank should be calculated as ceil(raw_rank), but the problem here is that
  // due to floating point calculation errors it can happen, that near int
  // values raw_rank won't be exactly X, but will be X.0000000000Y and if we
  // will use simple ceil function it will lead to wrong round up. So we will do
  // ceil ourself: extract int and fractional parts of the number and then
  // increase int part on one, but only if fractional big enough not to be an
  // floating point calculation error.
  double int_part;
  double fract_part = std::modf(raw_rank, &int_part);
  size_t rank = static_cast<size_t>(int_part);
  if (fract_part > kFloatingPointError) {
    rank++;
  }
  RTC_DCHECK_GT(rank, 0);
  RTC_DCHECK_LE(rank, samples_.size());
  return samples_[rank - 1];
}

}  // namespace webrtc
