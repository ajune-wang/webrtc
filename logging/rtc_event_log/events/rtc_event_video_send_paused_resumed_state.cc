/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_video_send_paused_resumed_state.h"

namespace webrtc {

RtcEventVideoSendPausedResumedState::RtcEventVideoSendPausedResumedState(
    bool is_paused)
    : is_paused_(is_paused) {}

RtcEvent::Type RtcEventVideoSendPausedResumedState::GetType() const {
  return RtcEvent::Type::VideoSendPausedResumedState;
}

bool RtcEventVideoSendPausedResumedState::IsConfigEvent() const {
  return false;
}

bool RtcEventVideoSendPausedResumedState::is_paused() const {
  return is_paused_;
}

}  // namespace webrtc
