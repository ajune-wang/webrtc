/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/environment/environment_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "api/make_ref_counted.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/transport/field_trial_based_config.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

EnvironmentFactory::EnvironmentFactory(const Environment& env)
    : leaf_(env.storage_),
      field_trials_(env.field_trials_),
      clock_(env.clock_),
      task_queue_factory_(env.task_queue_factory_),
      event_log_(env.event_log_) {}

Environment EnvironmentFactory::CreateWithDefaults() && {
  if (field_trials_ == nullptr) {
    With(std::make_unique<FieldTrialBasedConfig>());
  }
  if (clock_ == nullptr) {
    With(Clock::GetRealTimeClock());
  }
  if (task_queue_factory_ == nullptr) {
    With(CreateDefaultTaskQueueFactory(field_trials_));
  }
  if (event_log_ == nullptr) {
    With(std::make_unique<RtcEventLogNull>());
  }

  RTC_DCHECK(field_trials_ != nullptr);
  RTC_DCHECK(clock_ != nullptr);
  RTC_DCHECK(task_queue_factory_ != nullptr);
  RTC_DCHECK(event_log_ != nullptr);
  return Environment(std::move(leaf_),  //
                     field_trials_, clock_, task_queue_factory_, event_log_);
}

Environment EnvironmentFactory::Create() const {
  // Create a temporary copy to avoid mutating `this` with default utilities.
  return EnvironmentFactory(*this).CreateWithDefaults();
}

}  // namespace webrtc
