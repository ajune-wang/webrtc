/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/frame_delay_delta_kalman_filter.h"

#include "api/units/data_size.h"
#include "api/units/time_delta.h"

namespace webrtc {

namespace {
constexpr double kThetaLow = 0.000001;
}

FrameDelayDeltaKalmanFilter::FrameDelayDeltaKalmanFilter() {
  // TODO(brandtr): Is there a factor 1000 missing here?
  estimate_[0] = 1 / (512e3 / 8);  // Unit: [1 / bytes per ms]
  estimate_[1] = 0;                // Unit: [ms]

  // Initial estimate covariance.
  estimate_cov_[0][0] = 1e-4;  // Unit: [(1 / bytes per ms)^2]
  estimate_cov_[1][1] = 1e2;   // Unit: [ms^2]
  estimate_cov_[0][1] = estimate_cov_[1][0] = 0;

  // Process noise covariance.
  process_noise_cov_diag_[0] = 2.5e-10;  // Unit: [(1 / bytes per ms)^2]
  process_noise_cov_diag_[1] = 1e-10;    // Unit: [ms^2]
}

void FrameDelayDeltaKalmanFilter::PredictAndUpdate(
    TimeDelta frame_delay_variation,
    double frame_size_variation_bytes,
    DataSize max_frame_size,
    double var_noise) {
  // 1) Estimate prediction: There is no need to explicitly predict the
  // estimate, since the state transition matrix is the identity.

  // 2) Estimate covariance prediction: This is done by simply adding the
  // process noise covariance, again since the state transition matrix is the
  // identity.
  estimate_cov_[0][0] += process_noise_cov_diag_[0];
  estimate_cov_[1][1] += process_noise_cov_diag_[1];

  // 3) Innovation: The part of the measurement that cannot be explained by the
  // current estimate.
  double innovation =
      frame_delay_variation.ms() -
      GetFrameDelayVariationEstimateTotal(frame_size_variation_bytes);

  // 4) Innovation covariance.
  double estimate_cov_x_observation[2];
  estimate_cov_x_observation[0] =
      estimate_cov_[0][0] * frame_size_variation_bytes + estimate_cov_[0][1];
  estimate_cov_x_observation[1] =
      estimate_cov_[1][0] * frame_size_variation_bytes + estimate_cov_[1][1];
  // TODO(brandtr): Why is this check placed in the middle of this function?
  // Should it be at the top?
  if (max_frame_size < DataSize::Bytes(1)) {
    return;
  }
  double observation_noise_stddev =
      (300.0 * exp(-fabs(frame_size_variation_bytes) /
                   (1e0 * max_frame_size.bytes())) +
       1) *
      sqrt(var_noise);
  if (observation_noise_stddev < 1.0) {
    observation_noise_stddev = 1.0;
  }
  // TODO(brandtr): Shouldn't we add observation_noise_stddev^2 here? Otherwise,
  // the dimensional analysis fails.
  double innovation_cov =
      frame_size_variation_bytes * estimate_cov_x_observation[0] +
      estimate_cov_x_observation[1] + observation_noise_stddev;
  if ((innovation_cov < 1e-9 && innovation_cov >= 0) ||
      (innovation_cov > -1e-9 && innovation_cov <= 0)) {
    RTC_DCHECK_NOTREACHED();
    return;
  }

  // 5) Optimal Kalman gain: how much to trust the information in the
  // innovation.
  double kalman_gain[2];
  kalman_gain[0] = estimate_cov_x_observation[0] / innovation_cov;
  kalman_gain[1] = estimate_cov_x_observation[1] / innovation_cov;

  // 6) Estimate update.
  estimate_[0] += kalman_gain[0] * innovation;
  estimate_[1] += kalman_gain[1] * innovation;

  // This clamping is not part of the linear Kalman filter.
  if (estimate_[0] < kThetaLow) {
    estimate_[0] = kThetaLow;
  }

  // 7) Estimate covariance update.
  double t00 = estimate_cov_[0][0];
  double t01 = estimate_cov_[0][1];
  estimate_cov_[0][0] =
      (1 - kalman_gain[0] * frame_size_variation_bytes) * t00 -
      kalman_gain[0] * estimate_cov_[1][0];
  estimate_cov_[0][1] =
      (1 - kalman_gain[0] * frame_size_variation_bytes) * t01 -
      kalman_gain[0] * estimate_cov_[1][1];
  estimate_cov_[1][0] = estimate_cov_[1][0] * (1 - kalman_gain[1]) -
                        kalman_gain[1] * frame_size_variation_bytes * t00;
  estimate_cov_[1][1] = estimate_cov_[1][1] * (1 - kalman_gain[1]) -
                        kalman_gain[1] * frame_size_variation_bytes * t01;

  // Covariance matrix, must be positive semi-definite.
  RTC_DCHECK(estimate_cov_[0][0] + estimate_cov_[1][1] >= 0 &&
             estimate_cov_[0][0] * estimate_cov_[1][1] -
                     estimate_cov_[0][1] * estimate_cov_[1][0] >=
                 0 &&
             estimate_cov_[0][0] >= 0);
}

double FrameDelayDeltaKalmanFilter::GetFrameDelayVariationEstimateSizeBased(
    double frame_size_variation_bytes) const {
  // Unit: [1 / bytes per millisecond] * [bytes] = [milliseconds].
  return estimate_[0] * frame_size_variation_bytes;
}

double FrameDelayDeltaKalmanFilter::GetFrameDelayVariationEstimateTotal(
    double frame_size_variation_bytes) const {
  double frame_transmission_delay_ms =
      GetFrameDelayVariationEstimateSizeBased(frame_size_variation_bytes);
  double link_queuing_delay_ms = estimate_[1];
  return frame_transmission_delay_ms + link_queuing_delay_ms;
}

}  // namespace webrtc
