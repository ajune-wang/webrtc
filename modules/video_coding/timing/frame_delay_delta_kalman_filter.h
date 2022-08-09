/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMING_FRAME_DELAY_DELTA_KALMAN_FILTER_H_
#define MODULES_VIDEO_CODING_TIMING_FRAME_DELAY_DELTA_KALMAN_FILTER_H_

#include "api/units/data_size.h"
#include "api/units/time_delta.h"

namespace webrtc {

// This class uses a linear Kalman filter
// (see https://en.wikipedia.org/wiki/Kalman_filter) to estimate the frame delay
// delta (i.e., the difference in transmission time between a frame and the
// prior frame) for a frame, given its size delta in bytes (i.e., the difference
// in size between a frame and the prior frame). The idea is that, given a
// fixed network bandwidth, a larger frame (in bytes) would take proportionally
// longer to arrive than a correspondingly smaller frame. Using the variations
// of frame delay deltas and frame size deltas, the underlying bandwidth and
// propagation time of the network link can be estimated.
//
// The filter takes as input the frame delay delta and frame size delta, for a
// single frame. The hidden state is the network bandwidth and propagation
// delay. The estimated state can be used to get the expected frame delay delta
// for a frame, given it's frame size delta. This information can then be used
// to estimate the frame delay variation coming from network jitter.
class FrameDelayDeltaKalmanFilter {
 public:
  FrameDelayDeltaKalmanFilter();
  ~FrameDelayDeltaKalmanFilter() = default;
  // Update the Kalman filter with a measurement pair.

  //
  // This function will internally do both the prediction and the update steps.
  // Input:
  //          - frame_delay
  //              Delay-delta calculated by UTILDelayEstimate.
  //          - delta_frame_size_bytes
  //              Frame size delta, i.e. frame size at time T minus frame size
  //              at time T-1. (May be negative!)
  //          - max_frame_size
  //              Filtered version of the largest frame size received.
  //          - var_noise
  //              Variance of the estimated random jitter.
  void Update(TimeDelta frame_delay,
              double delta_frame_size_bytes,
              DataSize max_frame_size,
              double var_noise);

  // Calculates the difference in delay between a sample and the expected delay
  // estimated by the Kalman filter.
  //
  // Input:
  //          - frame_delay       : Delay-delta calculated by UTILDelayEstimate.
  //          - delta_frame_size_bytes : Frame size delta, i.e. frame size at
  //                                     time T minus frame size at time T-1.
  //
  // Return value               : The delay difference in ms.
  double DeviationFromExpectedDelay(TimeDelta frame_delay,
                                    double delta_frame_size_bytes) const;

  // Returns the estimated slope.
  double GetSlope() const;

 private:
  double theta_[2];         // Estimated line parameters (slope, offset)
  double theta_cov_[2][2];  // Estimate covariance
  double q_cov_[2][2];      // Process noise covariance
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_TIMING_FRAME_DELAY_DELTA_KALMAN_FILTER_H_
