/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ENVIRONMENT_ENVIRONMENT_FACTORY_H_
#define API_ENVIRONMENT_ENVIRONMENT_FACTORY_H_

#include <memory>

#include "absl/base/nullability.h"
#include "api/environment/environment.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// These classes are forward declared to reduce amount of headers exposed
// through api header.
class Clock;
class TaskQueueFactory;
class FieldTrialsView;
class RtcEventLog;

// Constructs `Environment`.
// Individual utilities are provided using one of the `With` functions.
// `With` functions do nothing when nullptr value is passed.
// Creates default implementations for utilities that are not provided.
//
// Examples:
//    Environment default_env = EnvironmentFactory().Create();
//    Environment custom_env =
//        EnvironmentFactory()
//            .With(std::make_unique<CustomTaskQueueFactory>())
//            .With(std::make_unique<CustomFieldTrials>())
//            .Create();
class RTC_EXPORT EnvironmentFactory final {
 public:
  EnvironmentFactory() = default;
  explicit EnvironmentFactory(const Environment& env);

  EnvironmentFactory(const EnvironmentFactory&) = default;
  EnvironmentFactory(EnvironmentFactory&&) = default;
  EnvironmentFactory& operator=(const EnvironmentFactory&) = default;
  EnvironmentFactory& operator=(EnvironmentFactory&&) = default;

  ~EnvironmentFactory() = default;

  EnvironmentFactory& With(  //
      absl::Nullable<std::unique_ptr<const FieldTrialsView>> field_trials);
  EnvironmentFactory& With(  //
      absl::Nullable<std::unique_ptr<Clock>> clock);
  EnvironmentFactory& With(  //
      absl::Nullable<std::unique_ptr<TaskQueueFactory>> task_queue_factory);
  EnvironmentFactory& With(  //
      absl::Nullable<std::unique_ptr<RtcEventLog>> event_log);

  EnvironmentFactory& With(  //
      absl::Nullable<const FieldTrialsView*> field_trials);
  EnvironmentFactory& With(  //
      absl::Nullable<Clock*> clock);
  EnvironmentFactory& With(  //
      absl::Nullable<TaskQueueFactory*> task_queue_factory);
  EnvironmentFactory& With(  //
      absl::Nullable<RtcEventLog*> event_log);

  Environment Create() const;

 private:
  Environment CreateWithDefaults() &&;

  scoped_refptr<const rtc::RefCountedBase> leaf_;

  absl::Nullable<const FieldTrialsView*> field_trials_ = nullptr;
  absl::Nullable<Clock*> clock_ = nullptr;
  absl::Nullable<TaskQueueFactory*> task_queue_factory_ = nullptr;
  absl::Nullable<RtcEventLog*> event_log_ = nullptr;
};

}  // namespace webrtc

#endif  // API_ENVIRONMENT_ENVIRONMENT_FACTORY_H_
