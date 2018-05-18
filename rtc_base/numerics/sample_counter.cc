/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/sample_counter.h"

namespace rtc {

SampleCounter::SampleCounter() : sum_(0), num_samples_(0) {}
SampleCounter::~SampleCounter() = default;

void SampleCounter::Add(int sample) {
  sum_ += sample;
  sum_squared_ += sample * sample;
  ++num_samples_;
  if (!max_ || sample > *max_) {
    max_.emplace(sample);
  }
}

void SampleCounter::Add(const SampleCounter& other) {
  sum_ += other.sum_;
  sum_squared_ += other.sum_squared_;
  num_samples_ += other.num_samples_;
  if (other.max_ && (!max_ || *max_ < *other.max_))
    max_ = other.max_;
}

int SampleCounter::Avg(int64_t min_required_samples) const {
  if (num_samples_ < min_required_samples || num_samples_ == 0)
    return -1;
  return static_cast<int>(sum_ / num_samples_);
}

int SampleCounter::StdDev(int64_t min_required_samples) const {
  if (num_samples_ < min_required_samples || num_samples_ == 0)
    return -1;
  return static_cast<int>(sum_squared_ / num_samples_ -
                          (sum_ / num_samples_) * (sum_ / num_samples_));
}

int SampleCounter::Max() const {
  return max_.value_or(-1);
}

void SampleCounter::Reset() {
  num_samples_ = 0;
  sum_ = 0;
  max_.reset();
}

}  // namespace rtc
