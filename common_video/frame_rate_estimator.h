/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_FRAME_RATE_ESTIMATOR_H_
#define COMMON_VIDEO_FRAME_RATE_ESTIMATOR_H_

#include <deque>

#include "absl/types/optional.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {

class FrameRateEstimator {
 public:
  explicit FrameRateEstimator(TimeDelta averaging_window);

  // Insert a frame, potentially culling old frames that falls outside the
  // averaging window.
  void OnFrame(Timestamp time);

  // Get the current average FPS, based on the frames currently in the window.
  absl::optional<double> GetAverageFps() const;

  // Move the window so it ends at |now|, and return the new fps estimate.
  absl::optional<double> GetAverageFps(Timestamp now);

  // Completely clear the averaging window.
  void Reset();

 private:
  void CullOld(Timestamp now);
  const TimeDelta averaging_window_;
  std::deque<Timestamp> frame_times_;
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_FRAME_RATE_ESTIMATOR_H_
