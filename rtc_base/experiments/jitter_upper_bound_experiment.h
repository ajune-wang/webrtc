/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EXPERIMENTS_JITTER_UPPER_BOUND_EXPERIMENT_H_
#define RTC_BASE_EXPERIMENTS_JITTER_UPPER_BOUND_EXPERIMENT_H_

#include "absl/types/optional.h"

namespace webrtc {

class JitterUpperBoundExperiment {
 public:
  // Returns nullopt if experiment is not on, otherwise returns the configured
  // upper bound for the jitter estimate, in milliseconds.
  static absl::optional<int> GetUpperBoundMs();

  static const char kJitterUpperBoundExperimentName[];
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_JITTER_UPPER_BOUND_EXPERIMENT_H_
