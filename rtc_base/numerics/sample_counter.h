/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_SAMPLE_COUNTER_H_
#define RTC_BASE_NUMERICS_SAMPLE_COUNTER_H_

#include "api/optional.h"

namespace rtc {

// Simple utility class for counting basic statistics (max./avg./std. dev.) on
// stream of samples.
class SampleCounter {
 public:
  SampleCounter();
  ~SampleCounter();

  void Add(int sample);
  // Returns -1 if less than |min_required_samples| were added.
  int Avg(int64_t min_required_samples) const;
  // Returns -1 if no samples were added.
  int StdDev(int64_t min_required_samples) const;
  // Returns -1 if no samples were added.
  int Max() const;
  void Reset();
  // Adds all the samples from the |other| SampleCounter as if they were all
  // individually added using |Add(int)| method.
  void Add(const SampleCounter& other);

 private:
  int64_t sum_;
  int64_t sum_squared_;
  int64_t num_samples_;
  rtc::Optional<int> max_;
};

}  // namespace rtc
#endif  // RTC_BASE_NUMERICS_SAMPLE_COUNTER_H_
