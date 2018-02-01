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
#include "rtc_base/rate_limiter.h"

namespace webrtc {
class Clock;

// This class converts network messages to legacy API. Note that this class is
// only safe to be used from a single thread.
class EncodingRateController {
 public:
  explicit EncodingRateController(const Clock* clock);
  ~EncodingRateController();
  void RegisterNetworkObserver(
      SendSideCongestionController::Observer* observer);
  void DeRegisterNetworkObserver(
      SendSideCongestionController::Observer* observer);

  void OnNetworkAvailability(NetworkAvailability msg);
  void OnTargetTransferRate(TargetTransferRate msg);
  void OnPacerQueueUpdate(PacerQueueUpdate msg);

 private:
  void OnNetworkInvalidation();

  bool GetNetworkParameters(int32_t* estimated_bitrate_bps,
                            uint8_t* fraction_loss,
                            int64_t* rtt_ms);
  bool IsSendQueueFull() const;
  bool HasNetworkParametersToReportChanged(int64_t bitrate_bps,
                                           uint8_t fraction_loss,
                                           int64_t rtt);
  SendSideCongestionController::Observer* observer_ = nullptr;

  rtc::Optional<TargetTransferRate> current_target_rate_msg_;

  bool network_available_ = true;
  int64_t last_reported_target_bitrate_bps_ = 0;
  uint8_t last_reported_fraction_loss_ = 0;
  int64_t last_reported_rtt_ms_ = 0;
  const bool pacer_pushback_experiment_ = false;
  int64_t pacer_expected_queue_ms_ = 0;
  float encoding_rate_ratio_ = 1.0;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EncodingRateController);
};
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_ENCODING_RATE_CONTROLLER_H_
