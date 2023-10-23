/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/connection_environment_builder.h"

#include <memory>
#include <utility>
#include <vector>

#include "api/make_ref_counted.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/transport/field_trial_based_config.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

ConnectionEnvironmentBuilder::ConnectionEnvironmentBuilder(
    const ConnectionEnvironment& env)
    : storage_(env.storage_),
      clock_(env.clock_),
      task_queue_factory_(env.task_queue_factory_),
      experiments_(env.experiments_),
      event_log_(env.event_log_) {}

ConnectionEnvironment ConnectionEnvironmentBuilder::Build() {
  if (clock_ == nullptr) {
    clock_ = Clock::GetRealTimeClock();
  }
  if (experiments_ == nullptr) {
    With(std::make_unique<FieldTrialBasedConfig>());
  }
  if (task_queue_factory_ == nullptr) {
    With(CreateDefaultTaskQueueFactory(experiments_));
  }
  if (event_log_ == nullptr) {
    With(std::make_unique<RtcEventLogNull>());
  }

  RTC_DCHECK(clock_ != nullptr);
  RTC_DCHECK(task_queue_factory_ != nullptr);
  RTC_DCHECK(experiments_ != nullptr);
  RTC_DCHECK(event_log_ != nullptr);
  return ConnectionEnvironment(storage_, experiments_, clock_,
                               task_queue_factory_, event_log_);
}

}  // namespace webrtc
