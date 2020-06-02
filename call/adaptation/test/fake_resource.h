/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_TEST_FAKE_RESOURCE_H_
#define CALL_ADAPTATION_TEST_FAKE_RESOURCE_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "call/adaptation/adaptation_constraint.h"
#include "call/adaptation/adaptation_listener.h"
#include "call/adaptation/resource.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/synchronization/sequence_checker.h"

namespace webrtc {

// TODO(hbos): BEFORE LANDING - split into different fakes!

// Fake resource used for testing.
class FakeResource : public Resource,
                     public AdaptationConstraint,
                     public AdaptationListener {
 public:
  static rtc::scoped_refptr<FakeResource> Create(std::string name);

  explicit FakeResource(std::string name);
  ~FakeResource() override;

  void set_usage_state(ResourceUsageState usage_state);
  void set_is_adaptation_up_allowed(bool is_adaptation_up_allowed);
  size_t num_adaptations_applied() const;

  // Resource implementation.
  std::string Name() const override;
  void SetResourceListener(ResourceListener* listener) override;
  absl::optional<ResourceUsageState> UsageState() const override;
  void ClearUsageState() override;

  // AdaptationConstraint implementation.
  bool IsAdaptationUpAllowed(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      rtc::scoped_refptr<Resource> reason_resource) const override;

  // AdaptationListener implementation.
  void OnAdaptationApplied(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      rtc::scoped_refptr<Resource> reason_resource) override;

 private:
  rtc::CriticalSection lock_;
  const std::string name_;
  ResourceListener* listener_ RTC_GUARDED_BY(lock_);
  absl::optional<ResourceUsageState> usage_state_ RTC_GUARDED_BY(lock_);
  bool is_adaptation_up_allowed_ RTC_GUARDED_BY(lock_);
  size_t num_adaptations_applied_ RTC_GUARDED_BY(lock_);
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_TEST_FAKE_RESOURCE_H_
