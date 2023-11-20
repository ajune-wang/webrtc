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
namespace {

template <typename T>
void Store(absl::Nonnull<std::unique_ptr<T>> value,
           scoped_refptr<const rtc::RefCountedBase>& leaf) {
  class StorageNode : public rtc::RefCountedBase {
   public:
    StorageNode(scoped_refptr<const rtc::RefCountedBase> parent,
                absl::Nonnull<std::unique_ptr<T>> value)
        : parent_(std::move(parent)), value_(std::move(value)) {}

    StorageNode(const StorageNode&) = delete;
    StorageNode& operator=(const StorageNode&) = delete;

    ~StorageNode() override = default;

   private:
    scoped_refptr<const rtc::RefCountedBase> parent_;
    absl::Nonnull<std::unique_ptr<T>> value_;
  };

  // Utilities provided with ownership would form a tree:
  // root is nullptr, each node keeps an ownership of one utility.
  // Each child node has a link to the parent, but parent is unaware of its
  // children. Each `EnvironmentFactory` and `Environment` keep a reference to a
  // 'leaf_' - node with the last provided utility. This way `Environment` keeps
  // ownership of a single branch of the storage tree with each used utiltity
  // owned by one of the nodes on that branch.
  leaf = rtc::make_ref_counted<StorageNode>(std::move(leaf), std::move(value));
}

}  // namespace

EnvironmentFactory::EnvironmentFactory(const Environment& env)
    : leaf_(env.storage_),
      field_trials_(env.field_trials_),
      clock_(env.clock_),
      task_queue_factory_(env.task_queue_factory_),
      event_log_(env.event_log_) {}

EnvironmentFactory& EnvironmentFactory::With(
    absl::Nullable<std::unique_ptr<const FieldTrialsView>> field_trials) {
  if (field_trials != nullptr) {
    field_trials_ = field_trials.get();
    Store(std::move(field_trials), leaf_);
  }
  return *this;
}

EnvironmentFactory& EnvironmentFactory::With(
    absl::Nullable<std::unique_ptr<Clock>> clock) {
  if (clock != nullptr) {
    clock_ = clock.get();
    Store(std::move(clock), leaf_);
  }
  return *this;
}

EnvironmentFactory& EnvironmentFactory::With(
    absl::Nullable<std::unique_ptr<TaskQueueFactory>> task_queue_factory) {
  if (task_queue_factory != nullptr) {
    task_queue_factory_ = task_queue_factory.get();
    Store(std::move(task_queue_factory), leaf_);
  }
  return *this;
}

EnvironmentFactory& EnvironmentFactory::With(
    absl::Nullable<std::unique_ptr<RtcEventLog>> event_log) {
  if (event_log != nullptr) {
    event_log_ = event_log.get();
    Store(std::move(event_log), leaf_);
  }
  return *this;
}

EnvironmentFactory& EnvironmentFactory::With(
    absl::Nullable<const FieldTrialsView*> field_trials) {
  if (field_trials != nullptr) {
    field_trials_ = field_trials;
  }
  return *this;
}

EnvironmentFactory& EnvironmentFactory::With(absl::Nullable<Clock*> clock) {
  if (clock != nullptr) {
    clock_ = clock;
  }
  return *this;
}

EnvironmentFactory& EnvironmentFactory::With(
    absl::Nullable<TaskQueueFactory*> task_queue_factory) {
  if (task_queue_factory != nullptr) {
    task_queue_factory_ = task_queue_factory;
  }
  return *this;
}

EnvironmentFactory& EnvironmentFactory::With(
    absl::Nullable<RtcEventLog*> event_log) {
  if (event_log != nullptr) {
    event_log_ = event_log;
  }
  return *this;
}

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
