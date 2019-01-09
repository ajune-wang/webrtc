/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_SAMPLES_STATS_COUNTER_H_
#define RTC_BASE_NUMERICS_SAMPLES_STATS_COUNTER_H_

#include <limits>
#include <vector>

namespace webrtc {

class SamplesStatsCounter {
 public:
  SamplesStatsCounter();
  ~SamplesStatsCounter();
  SamplesStatsCounter(SamplesStatsCounter&);
  SamplesStatsCounter(SamplesStatsCounter&&);

  // Adds sample to the stats in O(1).
  void AddSample(double value);

  // Returns if there are any values in O(1).
  bool IsEmpty() const;
  // Returns min in O(1). The caller may not call this function the stats are
  // empty.
  double GetMin() const;
  // Returns max in O(1). The caller may not call this function the stats are
  // empty.
  double GetMax() const;
  // Returns average in O(1). The caller may not call this function the stats
  // are empty.
  double GetAverage() const;
  // Returns percentile in O(nlogn) on first call and in O(1) after, if no
  // additions were done. The caller may not call this function the stats are
  // empty.
  double GetPercentile(double percentile);

 private:
  std::vector<double> samples_;
  double min_ = std::numeric_limits<double>::max();
  double max_ = std::numeric_limits<double>::min();
  double sum_ = 0;
  bool sorted_ = false;
};

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_SAMPLES_STATS_COUNTER_H_
