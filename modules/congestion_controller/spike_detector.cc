/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/spike_detector.h"

#include <math.h>

#include <algorithm>

#include "api/optional.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

namespace {

rtc::Optional<SpikeDetector::LineParameters> LinearFitSlope(
    std::deque<SpikeDetector::Point>::const_iterator begin,
    std::deque<SpikeDetector::Point>::const_iterator end) {
  // Compute the "center of mass".
  double sum_x = 0;
  double sum_y = 0;
  size_t num_points = 0;
  for (auto it = begin; it != end; ++it) {
    sum_x += it->x;
    sum_y += it->y;
    num_points++;
  }
  RTC_DCHECK(num_points >= 2);
  double x_avg = sum_x / num_points;
  double y_avg = sum_y / num_points;

  // Compute the slope k = \sum (x_i-x_avg)(y_i-y_avg) / \sum (x_i-x_avg)^2
  // Strictly speaking, we'd have to divide the following quantities by the
  // number of points to get the covariance and variances. We omit these
  // divisions since they will cancel out in the subsequent computations anyway.
  double cov_xy = 0;
  double var_x = 0;
  double var_y = 0;
  for (auto it = begin; it != end; ++it) {
    cov_xy += (it->x - x_avg) * (it->y - y_avg);
    var_x += (it->x - x_avg) * (it->x - x_avg);
    var_y += (it->y - y_avg) * (it->y - y_avg);
  }
  if (var_x == 0)
    return rtc::nullopt;
  double k = cov_xy / var_x;
  double m = y_avg - k * x_avg;
  // If the variance of the y values is zero then all of them are on a
  // horizontal line. Therefore, both the covariance and the error would also
  // be zero.
  double error = 0;
  if (var_y != 0)
    error = cov_xy * cov_xy / (var_x * var_y);  // TODO: This is the MSE.
  return SpikeDetector::LineParameters{k, m, error, num_points};
}

constexpr double kMaxAdaptOffsetMs = 0.0625;
constexpr double kOverUsingTimeThreshold = 10;
// constexpr int kMinNumDeltas = 60;

}  // namespace

enum { kDeltaCounterMax = 1000 };

SpikeDetector::SpikeDetector(size_t window_size,
                             size_t min_window_slice,
                             double min_threshold)
    : window_size_(window_size),
      min_window_slice_(min_window_slice),
      num_of_deltas_(0),
      first_arrival_time_ms_(-1),
      accumulated_delay_(0),
      delay_hist_(),
      first_trendline_{0, 0, 0, 0},
      second_trendline_{0, 0, 0, 0},
      k_up_(0.0087),
      k_down_(0.039),
      overusing_time_threshold_(kOverUsingTimeThreshold),
      threshold_(2 * min_threshold),
      min_threshold_(min_threshold),
      max_threshold_(100 * min_threshold),
      last_update_ms_(-1),
      prev_offset_(0.0),
      time_over_using_(-1),
      overuse_counter_(0),
      hypothesis_(BandwidthUsage::kBwNormal) {}

SpikeDetector::~SpikeDetector() {}

void SpikeDetector::Update(double recv_delta_ms,
                           double send_delta_ms,
                           int64_t arrival_time_ms) {
  const double delta_ms = recv_delta_ms - send_delta_ms;
  ++num_of_deltas_;
  if (num_of_deltas_ > kDeltaCounterMax)
    num_of_deltas_ = kDeltaCounterMax;
  if (first_arrival_time_ms_ == -1)
    first_arrival_time_ms_ = arrival_time_ms;

  // Accumulate the deltas to estimate the absolute delay.
  accumulated_delay_ += delta_ms;
  BWE_TEST_LOGGING_PLOT(1, "accumulated_delay_ms", arrival_time_ms,
                        accumulated_delay_);

  // Simple linear regression.
  delay_hist_.emplace_back(
      Point{static_cast<double>(arrival_time_ms - first_arrival_time_ms_),
            accumulated_delay_});
  if (delay_hist_.size() > window_size_)
    delay_hist_.pop_front();
  if (delay_hist_.size() == window_size_) {
    // Try all possible splits of the window and find the two trendlines that
    // best approximates the behavior over the entire window.
    double min_error = 1000000000.0;
    for (size_t split = 2; split <= window_size_ - 2; split++) {
      auto second_part_begin = delay_hist_.begin() + split;
      printf("%zu %zu\n", split, window_size_);
      auto first_trendline =
          LinearFitSlope(delay_hist_.begin(), second_part_begin);
      auto second_trendline =
          LinearFitSlope(second_part_begin, delay_hist_.end());
      if (first_trendline && second_trendline) {
        double sum_squared_error =
            split * first_trendline->error +
            (window_size_ - split) * second_trendline->error;
        if (sum_squared_error < min_error) {
          min_error = sum_squared_error;
          first_trendline_ = *first_trendline;
          second_trendline_ = *second_trendline;
        }
      }
    }
  }

  BWE_TEST_LOGGING_PLOT(1, "first_trendline_slope", arrival_time_ms,
                        first_trendline_.k);
  BWE_TEST_LOGGING_PLOT(1, "second_trendline_slope", arrival_time_ms,
                        second_trendline_.k);

  double trendline_slope;
  if (first_trendline_.num_points >= min_window_slice_ &&
      second_trendline_.num_points >= min_window_slice_) {
    trendline_slope = first_trendline_.error < second_trendline_.error
                          ? first_trendline_.k
                          : second_trendline_.k;
  } else {
    trendline_slope = first_trendline_.num_points > second_trendline_.num_points
                          ? first_trendline_.k
                          : second_trendline_.k;
  }

  BWE_TEST_LOGGING_PLOT(1, "trendline_slope", arrival_time_ms, trendline_slope);

  Detect(trendline_slope, send_delta_ms, num_of_deltas_, arrival_time_ms);
}

BandwidthUsage SpikeDetector::State() const {
  return hypothesis_;
}

BandwidthUsage SpikeDetector::Detect(double offset,
                                     double ts_delta,
                                     int num_of_deltas,
                                     int64_t now_ms) {
  if (num_of_deltas < 2) {
    return BandwidthUsage::kBwNormal;
  }
  const double T = offset;
  BWE_TEST_LOGGING_PLOT(1, "T", now_ms, T);
  BWE_TEST_LOGGING_PLOT(1, "threshold", now_ms, threshold_);
  if (T > threshold_) {
    if (time_over_using_ == -1) {
      // Initialize the timer. Assume that we've been
      // over-using half of the time since the previous
      // sample.
      time_over_using_ = ts_delta / 2;
    } else {
      // Increment timer
      time_over_using_ += ts_delta;
    }
    overuse_counter_++;
    if (time_over_using_ > overusing_time_threshold_ && overuse_counter_ > 1) {
      if (offset >= prev_offset_) {
        time_over_using_ = 0;
        overuse_counter_ = 0;
        hypothesis_ = BandwidthUsage::kBwOverusing;
      }
    }
  } else if (T < -threshold_) {
    time_over_using_ = -1;
    overuse_counter_ = 0;
    hypothesis_ = BandwidthUsage::kBwUnderusing;
  } else {
    time_over_using_ = -1;
    overuse_counter_ = 0;
    hypothesis_ = BandwidthUsage::kBwNormal;
  }
  prev_offset_ = offset;

  UpdateThreshold(T, now_ms);

  return hypothesis_;
}

void SpikeDetector::UpdateThreshold(double modified_offset, int64_t now_ms) {
  if (last_update_ms_ == -1)
    last_update_ms_ = now_ms;

  if (fabs(modified_offset) > threshold_ + kMaxAdaptOffsetMs) {
    // Avoid adapting the threshold to big latency spikes, caused e.g.,
    // by a sudden capacity drop.
    last_update_ms_ = now_ms;
    return;
  }

  const double k = fabs(modified_offset) < threshold_ ? k_down_ : k_up_;
  const int64_t kMaxTimeDeltaMs = 100;
  int64_t time_delta_ms = std::min(now_ms - last_update_ms_, kMaxTimeDeltaMs);
  threshold_ += k * (fabs(modified_offset) - threshold_) * time_delta_ms;
  threshold_ = rtc::SafeClamp(threshold_, min_threshold_, max_threshold_);
  last_update_ms_ = now_ms;
}

}  // namespace webrtc
