/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_SPIKE_DETECTOR_H_
#define MODULES_CONGESTION_CONTROLLER_SPIKE_DETECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <utility>

#include "modules/congestion_controller/delay_detector.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class SpikeDetector : public DelayDetector {
 public:
  struct Point {
    double x;
    double y;
  };

  struct LineParameters {
    double k;
    double m;
    double
        error;  // TODO: We might want mean_squared_error or something instead.
    size_t num_points;
  };

  // |window_size| is the number of points required to compute a trend line.
  // threshold_gain| is used to scale the trendline slope for comparison to
  // the old threshold.
  // TODO: FIX this: Once the old estimator has been removed (or the thresholds
  // been merged into the estimators), we can just set the threshold instead of
  // setting a gain.
  SpikeDetector(size_t window_size,
                size_t min_window_slice,
                double min_threshold);

  ~SpikeDetector() override;

  // Update the estimator with a new sample. The deltas should represent deltas
  // between timestamp groups as defined by the InterArrival class.
  void Update(double recv_delta_ms,
              double send_delta_ms,
              int64_t arrival_time_ms) override;

  BandwidthUsage State() const override;

 private:
  BandwidthUsage Detect(double offset,
                        double ts_delta,
                        int num_of_deltas,
                        int64_t now_ms);

  void UpdateThreshold(double modified_offset, int64_t now_ms);

  // Parameters.
  const size_t window_size_;
  const size_t min_window_slice_;
  // const double threshold_;
  // Used by the existing threshold.
  unsigned int num_of_deltas_;
  // Keep the arrival times small by using the change from the first packet.
  int64_t first_arrival_time_ms_;
  // Linear least squares regression.
  double accumulated_delay_;
  std::deque<Point> delay_hist_;
  LineParameters first_trendline_;
  LineParameters second_trendline_;

  const double k_up_;
  const double k_down_;
  double overusing_time_threshold_;
  double threshold_;
  const double min_threshold_;
  const double max_threshold_;
  int64_t last_update_ms_;
  double prev_offset_;
  double time_over_using_;
  int overuse_counter_;
  BandwidthUsage hypothesis_;

  RTC_DISALLOW_COPY_AND_ASSIGN(SpikeDetector);
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_SPIKE_DETECTOR_H_
