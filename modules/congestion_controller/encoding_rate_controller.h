/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
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

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;

 public:
  NetworkAvailability::MessageHandler NetworkAvailabilityReceiver;
  PacerQueueUpdate::MessageHandler PacerQueueUpdateReceiver;
  TargetTransferRate::MessageHandler TargetTransferRateReceiver;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EncodingRateController);
};
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_ENCODING_RATE_CONTROLLER_H_
