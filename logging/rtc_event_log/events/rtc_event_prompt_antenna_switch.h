/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_PROMPT_ANTENNA_SWITCH_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_PROMPT_ANTENNA_SWITCH_H_

#include "logging/rtc_event_log/events/rtc_event.h"

namespace webrtc {

class RtcEventPromptAntennaSwitch final : public RtcEvent {
 public:
  enum class ConnectionType { Wifi, Cellular };

  explicit RtcEventPromptAntennaSwitch(ConnectionType connectionType);
  ~RtcEventPromptAntennaSwitch() override = default;

  Type GetType() const override;

  static constexpr Type StaticType() {
    return RtcEvent::Type::PromptAntennaSwitch;
  }

  bool IsConfigEvent() const override;

  ConnectionType GetConnectionType() const;

 private:
  const ConnectionType connectionType_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_PROMPT_ANTENNA_SWITCH_H_
