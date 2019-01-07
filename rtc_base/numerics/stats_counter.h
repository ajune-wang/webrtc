/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_STATS_COUNTER_H_
#define RTC_BASE_NUMERICS_STATS_COUNTER_H_

#include <limits>
#include <vector>

namespace webrtc {

class StatsCounter {
 public:
  virtual ~StatsCounter() = default;

  // Adds sample to the stats.
  virtual void AddSample(double value) = 0;

  virtual bool HasValues() const = 0;
  // Returns min value or fails, if there are no values.
  virtual double GetMin() const = 0;
  // Returns max value or fails, if there are no values.
  virtual double GetMax() const = 0;
  // Returns average value or fails, if there are no values.
  virtual double GetAverage() const = 0;
  // Returns percentile value or fails, if there are no values.
  // |percentile| have to be in (0; 1].
  virtual double GetPercentile(double percentile) = 0;
};

// Implements stats counter by holding on values in the vector.
// This class IS NOT thread safe.
class BasicStatsCounter : public StatsCounter {
 public:
  BasicStatsCounter();
  ~BasicStatsCounter() override;

  // Adds sample in O(1).
  void AddSample(double value) override;

  // Returns in O(1).
  bool HasValues() const override;
  // Returns min in O(1).
  double GetMin() const override;
  // Returns max in O(1).
  double GetMax() const override;
  // Returns average in O(1).
  double GetAverage() const override;
  // Returns percentile in O(nlogn) on first call and in O(1) after, if no
  // additions were done.
  double GetPercentile(double percentile) override;

 private:
  std::vector<double> samples_;
  double min_ = std::numeric_limits<double>::max();
  double max_ = std::numeric_limits<double>::min();
  double sum_ = 0;
  bool sorted_ = false;
};

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_STATS_COUNTER_H_
