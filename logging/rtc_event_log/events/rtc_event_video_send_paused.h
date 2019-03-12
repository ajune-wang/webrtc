/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_VIDEO_SEND_PAUSED_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_VIDEO_SEND_PAUSED_H_

#include "logging/rtc_event_log/events/rtc_event.h"

namespace webrtc {

class RtcEventVideoSendPaused final : public RtcEvent {
 public:
  explicit RtcEventVideoSendPaused(bool isPaused);
  ~RtcEventVideoSendPaused() override = default;

  Type GetType() const override;

  static constexpr Type StaticType() { return RtcEvent::Type::VideoSendPaused; }

  bool IsConfigEvent() const override;

  bool IsPaused() const;

 private:
  const bool isPaused_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_VIDEO_SEND_PAUSED_H_
