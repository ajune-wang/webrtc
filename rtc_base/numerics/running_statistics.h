/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_RUNNING_STATISTICS_H_
#define RTC_BASE_NUMERICS_RUNNING_STATISTICS_H_

#include <cmath>
#include <limits>

#include "absl/types/optional.h"

namespace webrtc {

// tl;dr: Robust and efficient online computation of statistics,
//        using Welford's method for variance. [1]
//
// This should be your go-to class if you ever need to compute
// mean, variance and standard deviation.
// If you need to keep the data and/or get percentiles, please
// use webrtc::SamplesStatsCounter.
//
// [1]
// https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Welford's_online_algorithm

// The type T is a scalar which must be convertible to double.
// Rational: we often need greater precision for measures
//           than for the samples themselves.
template <typename T>
class RunningStatistics {
 public:
  // Update stats ////////////////////////////////////////////

  // Add a value participating in the statistics in O(1) time.
  void AddSample(T sample) {
    if (size_ == 0 || sample > max_) {
      max_ = sample;
    }
    if (size_ == 0 || sample < min_) {
      min_ = sample;
    }
    ++size_;
    // Welford's incremental update.
    const double delta = sample - mean_;
    mean_ += delta / size_;
    const double delta2 = sample - mean_;
    cumul_ += delta * delta2;
  }

  // Get Measures ////////////////////////////////////////////

  // Returns number of samples involved,
  // that is number of times AddSample() was called.
  int64_t Size() const { return size_; }

  // Returns min in O(1) time.
  absl::optional<T> GetMin() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return min_;
  }

  // Returns max in O(1) time.
  absl::optional<T> GetMax() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return max_;
  }

  // Returns mean in O(1) time.
  absl::optional<double> GetMean() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return mean_;
  }

  // Returns unbiased sample variance in O(1) time.
  absl::optional<double> GetVariance() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return cumul_ / size_;
  }

  // Returns unbiased standard deviation in O(1) time.
  absl::optional<double> GetStandardDeviation() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return std::sqrt(*GetVariance());
  }

 private:
  int64_t size_ = 0;  // Samples seen.
  T min_ = 0;         // Init value is never read. Quiet analyzers.
  T max_ = 0;         // Ditto.
  double mean_ = 0;
  double cumul_ = 0;  // Variance * size_, sometimes noted m2.
};

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_RUNNING_STATISTICS_H_
