/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_QUALITY_CONVERGENCE_MONITOR_H_
#define VIDEO_QUALITY_CONVERGENCE_MONITOR_H_

#include <deque>
#include <memory>

#include "api/video/video_codec_type.h"

namespace webrtc {

class QualityConvergenceMonitor {
 public:
  struct Parameters {
    int static_qp_threshold = 0;
    bool dynamic_detection_enabled = false;
    int dynamic_qp_threshold = 0;
    size_t window_length = 0;
    size_t tail_length = 0;
  };

  explicit QualityConvergenceMonitor(const Parameters& params);

  // Add the supplied `qp` value to the detection window.
  // `is_steady_state_refresh_frame` must only be `true` if the corresponding
  // video frame is a refresh frame that is used to improve the visual quality.
  void AddSample(int qp, bool is_steady_state_refresh_frame);

  // Returns `true` if the algorithm has determined that the supplied QP values
  // have converged and reached the target quality.
  bool AtTargetQuality() const;

 private:
  const Parameters params_;
  bool at_target_quality_ = false;

  // Contains a window of QP values. New values are added at the back while old
  // values are popped from the front to maintain the configured window length.
  std::deque<int> qp_window_;
};

}  // namespace webrtc

#endif  // VIDEO_QUALITY_CONVERGENCE_MONITOR_H_
