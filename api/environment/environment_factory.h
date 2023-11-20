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
#include <utility>

#include "absl/base/nullability.h"
#include "api/environment/environment.h"
#include "api/make_ref_counted.h"
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
// Ensures utilities provided with ownership will outlive all copies of the
// created `Environment`.
// Utilities passed to `EnvironmentFactory` without ownership are expected to be
// valid while any copy of the `Environment` object created by the
// `EnvironmentFactory` is alive.
// Creates default implementations for utilities that are not provided.
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

  template <typename T>
  EnvironmentFactory& With(absl::Nullable<std::unique_ptr<T>> utility);

  template <typename T>
  EnvironmentFactory& With(absl::Nullable<T*> utility);

  Environment Create() const;

 private:
  Environment CreateWithDefaults() &&;

  void Set(absl::Nonnull<const FieldTrialsView*> ptr) { field_trials_ = ptr; }
  void Set(absl::Nonnull<Clock*> ptr) { clock_ = ptr; }
  void Set(absl::Nonnull<TaskQueueFactory*> ptr) { task_queue_factory_ = ptr; }
  void Set(absl::Nonnull<RtcEventLog*> ptr) { event_log_ = ptr; }

  template <typename T>
  void Store(absl::Nonnull<std::unique_ptr<T>> utility);

  // Utilities provided with ownership would form a tree:
  // root is nullptr, each node keeps an ownership of one utility.
  // Each child node has a link to the parent, but parent is unaware of its
  // children. Each `EnvironmentFactory` and `Environment` keep a reference to a
  // 'leaf_' - node with the last provided utility. This way `Environment` keeps
  // ownership of a single branch of the storage tree with each used utiltity
  // owned by one of the nodes on that branch.
  scoped_refptr<const rtc::RefCountedBase> leaf_;

  absl::Nullable<const FieldTrialsView*> field_trials_ = nullptr;
  absl::Nullable<Clock*> clock_ = nullptr;
  absl::Nullable<TaskQueueFactory*> task_queue_factory_ = nullptr;
  absl::Nullable<RtcEventLog*> event_log_ = nullptr;
};

//------------------------------------------------------------------------------
// Implementation details follow
//------------------------------------------------------------------------------

template <typename T>
void EnvironmentFactory::Store(absl::Nonnull<std::unique_ptr<T>> utility) {
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

  leaf_ = rtc::make_ref_counted<StorageNode>(leaf_, std::move(utility));
}

template <typename T>
EnvironmentFactory& EnvironmentFactory::With(
    absl::Nullable<std::unique_ptr<T>> utility) {
  if (utility != nullptr) {
    Set(utility.get());
    Store(std::move(utility));
  }
  return *this;
}

template <typename T>
EnvironmentFactory& EnvironmentFactory::With(absl::Nullable<T*> utility) {
  if (utility != nullptr) {
    Set(utility);
  }
  return *this;
}

}  // namespace webrtc

#endif  // API_ENVIRONMENT_ENVIRONMENT_FACTORY_H_
