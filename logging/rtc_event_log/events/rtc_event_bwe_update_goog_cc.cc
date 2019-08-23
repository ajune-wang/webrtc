/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_bwe_update_goog_cc.h"

#include <stdint.h>

#include <memory>

#include "absl/memory/memory.h"

namespace webrtc {

RtcEventBweUpdateGoogCc::RtcEventBweUpdateGoogCc(
    int32_t target_bitrate_bps,
    const uint32_t delay_based_estimate_bps,
    const BandwidthUsage detector_state,
    const uint32_t loss_based_estimate_bps,
    const uint8_t fraction_loss)
    : target_rate_bps_(target_bitrate_bps),
      delay_based_estimate_bps_(delay_based_estimate_bps),
      detector_state_(detector_state),
      loss_based_estimate_bps_(loss_based_estimate_bps),
      fraction_loss_(fraction_loss) {}

RtcEventBweUpdateGoogCc::RtcEventBweUpdateGoogCc(
    const RtcEventBweUpdateGoogCc& other)
    : RtcEvent(other.timestamp_us_),
      target_rate_bps_(other.target_rate_bps_),
      delay_based_estimate_bps_(other.delay_based_estimate_bps_),
      detector_state_(other.detector_state_),
      loss_based_estimate_bps_(other.loss_based_estimate_bps_),
      fraction_loss_(other.fraction_loss_) {}

RtcEventBweUpdateGoogCc::~RtcEventBweUpdateGoogCc() = default;

RtcEvent::Type RtcEventBweUpdateGoogCc::GetType() const {
  return RtcEvent::Type::BweUpdateGoogCc;
}

bool RtcEventBweUpdateGoogCc::IsConfigEvent() const {
  return false;
}

std::unique_ptr<RtcEventBweUpdateGoogCc> RtcEventBweUpdateGoogCc::Copy() const {
  return absl::WrapUnique<RtcEventBweUpdateGoogCc>(
      new RtcEventBweUpdateGoogCc(*this));
}

}  // namespace webrtc
