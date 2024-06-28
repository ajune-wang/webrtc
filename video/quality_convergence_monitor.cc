/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/quality_convergence_monitor.h"

#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {

QualityConvergenceMonitor::QualityConvergenceMonitor(const Parameters& params)
    : params_(params) {
  RTC_CHECK(!params_.dynamic_detection_enabled ||
            params_.window_length > params_.tail_length);
}

void QualityConvergenceMonitor::AddSample(int qp,
                                          bool is_steady_state_refresh_frame) {
  // Invalid QP.
  if (qp < 0) {
    qp_window_.clear();
    at_target_quality_ = false;
    return;
  }

  if (qp <= params_.static_qp_threshold) {
    at_target_quality_ = true;
    return;
  }

  // Check for steady state and if dynamic detection is disabled.
  if (!is_steady_state_refresh_frame || !params_.dynamic_detection_enabled) {
    qp_window_.clear();
    at_target_quality_ = false;
    return;
  }

  // Check previous convergence.
  RTC_CHECK(is_steady_state_refresh_frame);
  if (at_target_quality_) {
    // No need to update state.
    return;
  }

  // Update QP history.
  qp_window_.push_back(qp);
  if (qp_window_.size() > params_.window_length) {
    qp_window_.pop_front();
  }

  // Check for sufficient data.
  if (qp_window_.size() <= params_.tail_length) {
    // No need to update state.
    RTC_CHECK(at_target_quality_ == false);
    return;
  }

  // Calculate average QP.
  float qp_head_average =
      std::accumulate(qp_window_.begin(),
                      qp_window_.end() - params_.tail_length, 0.0) /
      (qp_window_.size() - params_.tail_length);
  float qp_tail_average =
      std::accumulate(qp_window_.end() - params_.tail_length, qp_window_.end(),
                      0.0) /
      params_.tail_length;
  // Determine convergence.
  if (qp_head_average <= params_.dynamic_qp_threshold &&
      qp_head_average <= qp_tail_average) {
    at_target_quality_ = true;
  }
}

bool QualityConvergenceMonitor::AtTargetQuality() const {
  return at_target_quality_;
}

}  // namespace webrtc
