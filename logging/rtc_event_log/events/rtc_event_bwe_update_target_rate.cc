/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_bwe_update_target_rate.h"

#include <stdint.h>

#include <memory>

#include "absl/memory/memory.h"

namespace webrtc {

RtcEventBweUpdateTargetRate::RtcEventBweUpdateTargetRate(int32_t target_rate)
    : target_rate_(target_rate) {}

RtcEventBweUpdateTargetRate::RtcEventBweUpdateTargetRate(
    const RtcEventBweUpdateTargetRate& other)
    : RtcEvent(other.timestamp_us_), target_rate_(other.target_rate_) {}

RtcEventBweUpdateTargetRate::~RtcEventBweUpdateTargetRate() = default;

RtcEvent::Type RtcEventBweUpdateTargetRate::GetType() const {
  return RtcEvent::Type::BweUpdateTargetRate;
}

bool RtcEventBweUpdateTargetRate::IsConfigEvent() const {
  return false;
}

std::unique_ptr<RtcEventBweUpdateTargetRate> RtcEventBweUpdateTargetRate::Copy()
    const {
  return absl::WrapUnique<RtcEventBweUpdateTargetRate>(
      new RtcEventBweUpdateTargetRate(*this));
}

}  // namespace webrtc
