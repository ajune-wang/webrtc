/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_FAKE_RTC_EVENT_LOG_FACTORY_H_
#define LOGGING_RTC_EVENT_LOG_FAKE_RTC_EVENT_LOG_FACTORY_H_

#include <memory>

#include "api/umametrics.h"
#include "logging/rtc_event_log/fake_rtc_event_log.h"
#include "logging/rtc_event_log/rtc_event_log_factory_interface.h"
#include "rtc_base/thread.h"

namespace webrtc {

class FakeRtcEventLogFactory : public RtcEventLogFactoryInterface {
 public:
  explicit FakeRtcEventLogFactory(MetricsObserverInterface* observer)
      : observer_(observer), thread_(rtc::Thread::Current()) {}
  ~FakeRtcEventLogFactory() override {}

  std::unique_ptr<RtcEventLog> CreateRtcEventLog(
      RtcEventLog::EncodingType encoding_type) override;

  std::unique_ptr<RtcEventLog> CreateRtcEventLog(
      RtcEventLog::EncodingType encoding_type,
      std::unique_ptr<rtc::TaskQueue> task_queue) override;

  MetricsObserverInterface* observer() const { return observer_; }
  rtc::Thread* thread() { return thread_; }

 private:
  MetricsObserverInterface* observer_;
  rtc::Thread* thread_;
};

std::unique_ptr<RtcEventLogFactoryInterface> CreateFakeRtcEventLogFactory(
    MetricsObserverInterface* observer);
}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_FAKE_RTC_EVENT_LOG_FACTORY_H_
