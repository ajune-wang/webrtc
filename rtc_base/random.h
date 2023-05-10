/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_RANDOM_H_
#define RTC_BASE_RANDOM_H_

#include <stdint.h>

#include <limits>
#include <type_traits>

#include "absl/random/random.h"

namespace webrtc {

class Random {
 public:
  // TODO(tommi): Change this so that the seed can be initialized internally,
  // e.g. by offering two ways of constructing or offer a static method that
  // returns a seed that's suitable for initialization.
  // The problem now is that callers are calling clock_->TimeInMicroseconds()
  // which calls TickTime::Now().Ticks(), which can return a very low value on
  // Mac and can result in a seed of 0 after conversion to microseconds.
  // Besides the quality of the random seed being poor, this also requires
  // the client to take on extra dependencies to generate a seed.
  // If we go for a static seed generator in Random, we can use something from
  // webrtc/rtc_base and make sure that it works the same way across platforms.
  // See also discussion here: https://codereview.webrtc.org/1623543002/
  explicit Random(uint64_t /*seed*/) {}

  Random() = delete;
  Random(const Random&) = delete;
  Random& operator=(const Random&) = delete;

  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, bool>, T> Rand() {
    return absl::Bernoulli(bitgen_, 0.5);
  }

  template <typename T>
  typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>,
                            T>
  Rand() {
    return absl::Uniform<T>(absl::IntervalClosed, bitgen_,
                            std::numeric_limits<T>::min(),
                            std::numeric_limits<T>::max());
  }

  template <typename T>
  typename std::enable_if_t<std::is_floating_point_v<T>, T> Rand() {
    return absl::Uniform<T>(absl::IntervalClosed, bitgen_, 0, 1);
  }

  // Uniformly distributed pseudo-random number in the interval [0, t].
  uint32_t Rand(uint32_t t) {
    return absl::Uniform(absl::IntervalClosed, bitgen_, uint32_t{0}, t);
  }

  // Uniformly distributed pseudo-random number in the interval [low, high].
  uint32_t Rand(uint32_t low, uint32_t high) {
    return absl::Uniform(absl::IntervalClosed, bitgen_, low, high);
  }

  // Uniformly distributed pseudo-random number in the interval [low, high].
  int32_t Rand(int32_t low, int32_t high) {
    return absl::Uniform(absl::IntervalClosed, bitgen_, low, high);
  }

  // Normal Distribution.
  double Gaussian(double mean, double standard_deviation) {
    return absl::Gaussian(bitgen_, mean, standard_deviation);
  }

  // Exponential Distribution.
  double Exponential(double lambda) {
    return absl::Exponential(bitgen_, lambda);
  }

 private:
  absl::BitGen bitgen_;
};

}  // namespace webrtc

#endif  // RTC_BASE_RANDOM_H_
