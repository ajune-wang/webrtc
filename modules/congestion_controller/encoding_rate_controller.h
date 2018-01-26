/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_ENCODING_RATE_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_ENCODING_RATE_CONTROLLER_H_
#include <memory>

#include "modules/congestion_controller/include/send_side_congestion_controller.h"
#include "network_control/include/network_types.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/task_queue.h"

namespace webrtc {
class Clock;

// This class converts network messages to legacy API
class EncodingRateController {
 public:
  explicit EncodingRateController(const Clock* clock);
  ~EncodingRateController();
  void RegisterNetworkObserver(
      SendSideCongestionController::Observer* observer);
  void DeRegisterNetworkObserver(
      SendSideCongestionController::Observer* observer);
  RateLimiter* GetRetransmissionRateLimiter();
  void OnNetworkAvailability(NetworkAvailability);
  void OnTargetTransferRate(TargetTransferRate);
  void OnPacerQueueUpdate(PacerQueueUpdate);

 private:
  void OnNetworkInvalidation();

  bool GetNetworkParameters(int32_t* estimated_bitrate_bps,
                            uint8_t* fraction_loss,
                            int64_t* rtt_ms);
  bool IsSendQueueFull() const;
  bool HasNetworkParametersToReportChanged(int64_t bitrate_bps,
                                           uint8_t fraction_loss,
                                           int64_t rtt);
  rtc::CriticalSection observer_lock_;
  SendSideCongestionController::Observer* observer_
      RTC_GUARDED_BY(observer_lock_) = nullptr;
  const std::unique_ptr<RateLimiter> retransmission_rate_limiter_;

  rtc::Optional<TargetTransferRate> current_target_rate_msg_;

  bool network_available_ = true;
  int64_t last_reported_target_bitrate_bps_;
  uint8_t last_reported_fraction_loss_;
  int64_t last_reported_rtt_ms_;
  bool pacer_pushback_experiment_ = false;
  int64_t pacer_expected_queue_ms_ = 0;
  float encoding_rate_ = 1.0;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EncodingRateController);
};
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_ENCODING_RATE_CONTROLLER_H_
