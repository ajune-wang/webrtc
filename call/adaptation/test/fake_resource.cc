/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/test/fake_resource.h"

#include <algorithm>
#include <utility>

#include "rtc_base/ref_counted_object.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {

// static
rtc::scoped_refptr<FakeResource> FakeResource::Create(std::string name) {
  return new rtc::RefCountedObject<FakeResource>(name);
}

FakeResource::FakeResource(std::string name)
    : Resource(),
      lock_(),
      name_(std::move(name)),
      listener_(nullptr),
      usage_state_(absl::nullopt),
      is_adaptation_up_allowed_(true),
      num_adaptations_applied_(0) {}

FakeResource::~FakeResource() {}

void FakeResource::set_usage_state(ResourceUsageState usage_state) {
  rtc::CritScope crit(&lock_);
  usage_state_ = usage_state;
  if (listener_) {
    listener_->OnResourceUsageStateMeasured(this);
  }
}

void FakeResource::set_is_adaptation_up_allowed(bool is_adaptation_up_allowed) {
  rtc::CritScope crit(&lock_);
  is_adaptation_up_allowed_ = is_adaptation_up_allowed;
}

size_t FakeResource::num_adaptations_applied() const {
  rtc::CritScope crit(&lock_);
  return num_adaptations_applied_;
}

std::string FakeResource::Name() const {
  return name_;
}

void FakeResource::SetResourceListener(ResourceListener* listener) {
  rtc::CritScope crit(&lock_);
  listener_ = listener;
}

absl::optional<ResourceUsageState> FakeResource::UsageState() const {
  rtc::CritScope crit(&lock_);
  return usage_state_;
}

void FakeResource::ClearUsageState() {
  rtc::CritScope crit(&lock_);
  usage_state_ = absl::nullopt;
}

bool FakeResource::IsAdaptationUpAllowed(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    rtc::scoped_refptr<Resource> reason_resource) const {
  rtc::CritScope crit(&lock_);
  return is_adaptation_up_allowed_;
}

void FakeResource::OnAdaptationApplied(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    rtc::scoped_refptr<Resource> reason_resource) {
  rtc::CritScope crit(&lock_);
  ++num_adaptations_applied_;
}

}  // namespace webrtc
