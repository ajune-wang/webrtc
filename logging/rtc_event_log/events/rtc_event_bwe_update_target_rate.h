/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_BWE_UPDATE_TARGET_RATE_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_BWE_UPDATE_TARGET_RATE_H_

#include <stdint.h>

#include <memory>

#include "logging/rtc_event_log/events/rtc_event.h"

namespace webrtc {

class RtcEventBweUpdateTargetRate : public RtcEvent {
 public:
  explicit RtcEventBweUpdateTargetRate(int32_t target_rate);
  ~RtcEventBweUpdateTargetRate() override;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  std::unique_ptr<RtcEventBweUpdateTargetRate> Copy() const;

  int32_t target_rate() const { return target_rate_; }

 private:
  RtcEventBweUpdateTargetRate(const RtcEventBweUpdateTargetRate&);

  const int32_t target_rate_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_BWE_UPDATE_TARGET_RATE_H_
