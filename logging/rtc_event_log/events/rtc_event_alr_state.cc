/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_alr_state.h"

namespace webrtc {

RtcEventAlrState::RtcEventAlrState(bool in_alr, int alr_budget)
    : in_alr_(in_alr), alr_budget_(alr_budget) {}

RtcEventAlrState::~RtcEventAlrState() = default;

RtcEvent::Type RtcEventAlrState::GetType() const {
  return RtcEvent::Type::AlrStateEvent;
}

bool RtcEventAlrState::IsConfigEvent() const {
  return false;
}

}  // namespace webrtc
