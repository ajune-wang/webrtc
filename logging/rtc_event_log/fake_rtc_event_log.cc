/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/fake_rtc_event_log.h"

#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

bool FakeRtcEventLog::StartLogging(
    std::unique_ptr<RtcEventLogOutput> output,
    int64_t output_period_ms) {
  return true;
}

void FakeRtcEventLog::StopLogging() {
  invoker_.Flush(thread_);
}

void FakeRtcEventLog::Log(std::unique_ptr<RtcEvent> event) {
  RtcEvent::Type rtc_event_type = event->GetType();
  RTC_DCHECK(thread_);
  RTC_DCHECK(observer_);
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, thread_,
      rtc::Bind(&MetricsObserverInterface::IncrementEnumCounter, observer_,
                kEnumCounterIceEventLog, static_cast<int>(rtc_event_type),
                static_cast<int>(IceCandidatePairEventType::kNumTypes)));
}

}  // namespace webrtc
