/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_FAKE_RTC_EVENT_LOG_H_
#define LOGGING_RTC_EVENT_LOG_FAKE_RTC_EVENT_LOG_H_

#include <memory>

#include "api/umametrics.h"
#include "logging/rtc_event_log/events/rtc_event.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/thread.h"

namespace webrtc {

class FakeRtcEventLog : public RtcEventLog {
 public:
  FakeRtcEventLog(MetricsObserverInterface* observer, rtc::Thread* thread)
      : observer_(observer), thread_(thread) {}
  bool StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                    int64_t output_period_ms) override;
  void StopLogging() override;
  void Log(std::unique_ptr<RtcEvent> event) override;

 private:
  // All methods of the metric observer should be called on the same thread
  // except the constructor.
  MetricsObserverInterface* observer_;
  rtc::Thread* thread_;
  rtc::AsyncInvoker invoker_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_FAKE_RTC_EVENT_LOG_H_
