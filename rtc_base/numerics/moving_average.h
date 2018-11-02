/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_MOVING_AVERAGE_H_
#define RTC_BASE_NUMERICS_MOVING_AVERAGE_H_

#include <vector>

#include "absl/types/optional.h"

namespace rtc {

// Calculates average over fixed size window. If there are less than window
// size elements, calculates average of all inserted so far elements.
//
class MovingAverage {
 public:
  explicit MovingAverage(size_t window_size);
  ~MovingAverage();

  void AddSample(int sample);
  // Returns rounded down average of last |window_size| elements or all
  // elements if there are not enough of them. Returns nullopt if there were
  // no elements added.
  absl::optional<int> GetAverageRoundedDown() const;

  // Same as GetAverage but rounded to the closest integer.
  absl::optional<int> GetAverageRoundedToClosest() const;

  // Resets to the initial state before any elements were added.
  void Reset();

  // Returns number of elements in the window.
  size_t size() const;

  // MovingAverage is neither copyable nor movable.
  MovingAverage(const MovingAverage&) = delete;
  MovingAverage& operator=(const MovingAverage&) = delete;

 private:
  double GetUnroundedAverage() const;

  size_t count_ = 0;
  int64_t sum_ = 0;
  std::vector<int> history_;
};

}  // namespace rtc
#endif  // RTC_BASE_NUMERICS_MOVING_AVERAGE_H_
