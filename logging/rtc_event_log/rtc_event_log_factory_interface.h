/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_INTERFACE_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_INTERFACE_H_

#include <memory>

#include "api/task_queue/task_queue_factory.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "rtc_base/deprecation.h"

namespace webrtc {

// This interface exists to allow webrtc to be optionally built without
// RtcEventLog support. A PeerConnectionFactory is constructed with an
// RtcEventLogFactoryInterface, which may or may not be null.
class RtcEventLogFactoryInterface {
 public:
  virtual ~RtcEventLogFactoryInterface() {}

  // TODO(bugs.webrtc.org/10284): Remove when unused.
  RTC_DEPRECATED
  std::unique_ptr<RtcEventLog> CreateRtcEventLog(
      RtcEventLog::EncodingType encoding_type);

  virtual std::unique_ptr<RtcEventLog> CreateRtcEventLog(
      RtcEventLog::EncodingType encoding_type,
      TaskQueueFactory* task_queue_factory) = 0;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_INTERFACE_H_
