/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_ALR_DETECTOR_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_ALR_DETECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/units/data_rate.h"
#include "modules/pacing/interval_budget.h"

namespace webrtc {

class RtcEventLog;

// Application limited region detector is a class that utilizes signals of
// elapsed time and bytes sent to estimate whether network traffic is
// currently limited by the application's ability to generate traffic.
//
// AlrDetector provides a signal that can be utilized to adjust
// estimate bandwidth.
// Note: This class is not thread-safe.
class AlrDetector {
 public:
  AlrDetector();
  explicit AlrDetector(RtcEventLog* event_log);
  ~AlrDetector();

  void OnBytesSent(size_t bytes_sent, int64_t send_time_ms);

  // Set current estimated bandwidth.
  void SetEstimatedBitrate(int bitrate_bps);

  // Enters ALR when the estimate reach |rate| or above. Used when it is known
  // that an application can not generate arbitrary traffic rates at a higher
  // rate than |rate|.
  void StartAlrAtEstimatedRate(DataRate rate);

  // Returns time in milliseconds when the current application-limited region
  // started or empty result if the sender is currently not application-limited.
  absl::optional<int64_t> GetApplicationLimitedRegionStartTime() const;

  // Sent traffic percentage as a function of network capacity used to determine
  // application-limited region. ALR region start when bandwidth usage drops
  // below kAlrStartUsagePercent and ends when it raises above
  // kAlrEndUsagePercent. NOTE: This is intentionally conservative at the moment
  // until BW adjustments of application limited region is fine tuned.
  static constexpr int kDefaultAlrBandwidthUsagePercent = 65;
  static constexpr int kDefaultAlrStartBudgetLevelPercent = 80;
  static constexpr int kDefaultAlrStopBudgetLevelPercent = 50;

  void UpdateBudgetWithElapsedTime(int64_t delta_time_ms);
  void UpdateBudgetWithBytesSent(size_t bytes_sent);

 private:
  void MaybeChangeState();

  friend class GoogCcStatePrinter;
  int bandwidth_usage_percent_;
  int alr_start_budget_level_percent_;
  int alr_stop_budget_level_percent_;
  DataRate auto_start_alr_at_bwe_;
  DataRate bitrate_estimate_;

  absl::optional<int64_t> last_send_time_ms_;

  IntervalBudget alr_budget_;
  absl::optional<int64_t> alr_started_time_ms_;

  RtcEventLog* event_log_;
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_ALR_DETECTOR_H_
