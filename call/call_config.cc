/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/call_config.h"

#include "rtc_base/checks.h"

namespace webrtc {

CallConfig::CallConfig(RtcEventLog* event_log,
                       TaskQueueFactory* task_queue_factory)
    : event_log(event_log), task_queue_factory(task_queue_factory) {
  RTC_DCHECK(event_log);
  RTC_DCHECK(task_queue_factory);
}

CallConfig::CallConfig(const CallConfig& config) = default;

CallConfig::~CallConfig() = default;

}  // namespace webrtc
