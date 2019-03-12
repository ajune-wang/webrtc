/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "logging/rtc_event_log/events/rtc_event_prompt_antenna_switch.h"

namespace webrtc {

RtcEventPromptAntennaSwitch::RtcEventPromptAntennaSwitch(
    RtcEventPromptAntennaSwitch::ConnectionType connectionType)
    : connectionType_(connectionType) {}

RtcEvent::Type RtcEventPromptAntennaSwitch::GetType() const {
  return StaticType();
}

bool RtcEventPromptAntennaSwitch::IsConfigEvent() const {
  return false;
}

RtcEventPromptAntennaSwitch::ConnectionType
RtcEventPromptAntennaSwitch::GetConnectionType() const {
  return connectionType_;
}

}  // namespace webrtc
