/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_VIDEO_SEND_PAUSED_RESUMED_STATE_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_VIDEO_SEND_PAUSED_RESUMED_STATE_H_

#include "logging/rtc_event_log/events/rtc_event.h"

namespace webrtc {

class RtcEventVideoSendPausedResumedState final : public RtcEvent {
 public:
  explicit RtcEventVideoSendPausedResumedState(bool is_paused);
  ~RtcEventVideoSendPausedResumedState() override = default;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  bool is_paused() const;

 private:
  const bool is_paused_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_VIDEO_SEND_PAUSED_RESUMED_STATE_H_
