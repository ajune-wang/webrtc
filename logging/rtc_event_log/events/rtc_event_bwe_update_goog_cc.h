/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_BWE_UPDATE_GOOG_CC_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_BWE_UPDATE_GOOG_CC_H_

#include <stdint.h>

#include <memory>

#include "logging/rtc_event_log/events/rtc_event.h"

namespace webrtc {

enum class BandwidthUsage;

class RtcEventBweUpdateGoogCc : public RtcEvent {
 public:
  explicit RtcEventBweUpdateGoogCc(int32_t target_bitrate_bps,
                                   uint32_t delay_based_estimate_bps,
                                   BandwidthUsage detector_state,
                                   uint32_t loss_based_estimate_bps,
                                   uint8_t fraction_loss);
  ~RtcEventBweUpdateGoogCc() override;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  std::unique_ptr<RtcEventBweUpdateGoogCc> Copy() const;

  int32_t target_rate_bps() const { return target_rate_bps_; }
  uint32_t delay_based_estimate_bps() const {
    return delay_based_estimate_bps_;
  }
  BandwidthUsage detector_state() const { return detector_state_; }
  uint32_t loss_based_estimate_bps() const { return loss_based_estimate_bps_; }
  uint8_t fraction_loss() const { return fraction_loss_; }

 private:
  RtcEventBweUpdateGoogCc(const RtcEventBweUpdateGoogCc&);

  const int32_t target_rate_bps_;
  const uint32_t delay_based_estimate_bps_;
  const BandwidthUsage detector_state_;
  const uint32_t loss_based_estimate_bps_;
  const uint8_t fraction_loss_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_BWE_UPDATE_GOOG_CC_H_
