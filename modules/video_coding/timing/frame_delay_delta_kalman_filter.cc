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
constexpr double kMinAllowedSlopeEstimate = 0.000001;
}

FrameDelayDeltaKalmanFilter::FrameDelayDeltaKalmanFilter() {
  // Initial estimate.
  estimate_[0] = 1 / (512e3 / 8);  // Link bandwidth [1 / bytes per ms].
  estimate_[1] = 0;                // Link propagation delay [ms].

  // Initial estimate covariance.
  estimate_cov_[0][0] = 1e-4;
  estimate_cov_[1][1] = 1e2;
  estimate_cov_[0][1] = estimate_cov_[1][0] = 0;

  // Initial process noise covariance.
  process_noise_cov_[0][0] = 2.5e-10;
  process_noise_cov_[1][1] = 1e-10;
  process_noise_cov_[0][1] = process_noise_cov_[1][0] =
      0;  // TODO(brandtr): Remove.
}

void FrameDelayDeltaKalmanFilter::PredictAndUpdate(
    TimeDelta frame_delay_delta,
    double frame_size_delta_bytes,
    DataSize max_frame_size,
    double var_noise) {
  Predict();
  Update(frame_delay_delta, frame_size_delta_bytes, max_frame_size, var_noise);
}

void FrameDelayDeltaKalmanFilter::Predict() {
  RTC_DCHECK(logical_state_ == LogicalState::kInitialized ||
             logical_state_ == LogicalState::kUpdated);

  // Estimate prediction: There is no need to explicitly predict the estimate,
  // since the state transition matrix is the identity.

  // Estimate covariance prediction: This is done by simply adding the process
  // noise covariance, again since the state transition matrix is the identity.
  estimate_cov_[0][0] += process_noise_cov_[0][0];
  estimate_cov_[0][1] += process_noise_cov_[0][1];  // TODO(brandtr): Remove.
  estimate_cov_[1][0] += process_noise_cov_[1][0];
  estimate_cov_[1][1] += process_noise_cov_[1][1];  // TODO(brandtr): Remove.

  logical_state_ = LogicalState::kPredicted;
}

void FrameDelayDeltaKalmanFilter::Update(TimeDelta frame_delay_delta,
                                         double frame_size_delta_bytes,
                                         DataSize max_frame_size,
                                         double var_noise) {
  RTC_DCHECK_EQ(logical_state_, LogicalState::kPredicted);

  // Innovation: the part of the measurement that is not explained by the
  // current filter state.
  double innovation = frame_delay_delta.ms() -
                      GetFrameDelayDeltaEstimateTotal(frame_size_delta_bytes);

  // Innovation covariance:
  double Ph[2];
  Ph[0] = estimate_cov_[0][0] * frame_size_delta_bytes + estimate_cov_[0][1];
  Ph[1] = estimate_cov_[1][0] * frame_size_delta_bytes + estimate_cov_[1][1];
  // `r` weights measurements with a small deltaFS as noisy and
  // measurements with large deltaFS as good
  if (max_frame_size < DataSize::Bytes(1)) {
    return;
  }
  double r = (300.0 * exp(-fabs(frame_size_delta_bytes) /
                          (1e0 * max_frame_size.bytes())) +
              1) *
             sqrt(var_noise);
  if (r < 1.0) {
    r = 1.0;
  }
  double hPh_r = frame_size_delta_bytes * Ph[0] + Ph[1] + r;
  if ((hPh_r < 1e-9 && hPh_r >= 0) || (hPh_r > -1e-9 && hPh_r <= 0)) {
    RTC_DCHECK_NOTREACHED();
    return;
  }

  // Optimal Kalman gain.
  double kalman_gain[2];
  kalman_gain[0] = Ph[0] / hPh_r;
  kalman_gain[1] = Ph[1] / hPh_r;

  // Updated (a posteriori) state estimate.
  estimate_[0] += kalman_gain[0] * innovation;
  estimate_[1] += kalman_gain[1] * innovation;

  if (estimate_[0] < kMinAllowedSlopeEstimate) {
    estimate_[0] = kMinAllowedSlopeEstimate;
  }

  // Updated (a posteriori) estimate covariance
  // P = (I - K*h)*P
  double t00, t01;
  t00 = estimate_cov_[0][0];
  t01 = estimate_cov_[0][1];
  estimate_cov_[0][0] = (1 - kalman_gain[0] * frame_size_delta_bytes) * t00 -
                        kalman_gain[0] * estimate_cov_[1][0];
  estimate_cov_[0][1] = (1 - kalman_gain[0] * frame_size_delta_bytes) * t01 -
                        kalman_gain[0] * estimate_cov_[1][1];
  estimate_cov_[1][0] = estimate_cov_[1][0] * (1 - kalman_gain[1]) -
                        kalman_gain[1] * frame_size_delta_bytes * t00;
  estimate_cov_[1][1] = estimate_cov_[1][1] * (1 - kalman_gain[1]) -
                        kalman_gain[1] * frame_size_delta_bytes * t01;

  // Covariance matrix, must be positive semi-definite.
  RTC_DCHECK(estimate_cov_[0][0] + estimate_cov_[1][1] >= 0 &&
             estimate_cov_[0][0] * estimate_cov_[1][1] -
                     estimate_cov_[0][1] * estimate_cov_[1][0] >=
                 0 &&
             estimate_cov_[0][0] >= 0);

  logical_state_ = LogicalState::kUpdated;
}

double FrameDelayDeltaKalmanFilter::GetFrameDelayDeltaEstimateSizeBased(
    double frame_size_delta_bytes) const {
  RTC_DCHECK_NE(logical_state_, LogicalState::kPredicted);
  // Unit: [1 / bytes per millisecond] * [bytes] = [milliseconds].
  return estimate_[0] * frame_size_delta_bytes;
}

double FrameDelayDeltaKalmanFilter::GetFrameDelayDeltaEstimateTotal(
    double frame_size_delta_bytes) const {
  RTC_DCHECK_NE(logical_state_, LogicalState::kPredicted);
  double frame_transmission_delay_ms =
      GetFrameDelayDeltaEstimateSizeBased(frame_size_delta_bytes);
  double link_propagation_delay_ms = estimate_[1];
  return frame_transmission_delay_ms + link_propagation_delay_ms;
}

}  // namespace webrtc
