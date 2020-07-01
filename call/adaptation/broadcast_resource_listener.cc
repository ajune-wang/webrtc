/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/broadcast_resource_listener.h"

#include <algorithm>
#include <string>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

// The AdapterResource redirects resource usage measurements from its parent to
// a single ResourceListener. When its listener is unregistered, the
// AdapterResource is unregistered from its parent - it cleans up after itself.
class BroadcastResourceListener::AdapterResource : public Resource {
 public:
  AdapterResource(BroadcastResourceListener* parent, std::string name)
      : parent_(parent), name_(std::move(name)) {
    RTC_DCHECK(parent_);
  }
  ~AdapterResource() override { RTC_DCHECK(!listener_); }

  // The parent is letting us know we have a usage neasurement.
  void OnResourceUsageStateMeasured(ResourceUsageState usage_state) {
    rtc::CritScope crit(&lock_);
    if (!listener_)
      return;
    listener_->OnResourceUsageStateMeasured(this, usage_state);
  }

  // Resource implementation.
  std::string Name() const override { return name_; }
  void SetResourceListener(ResourceListener* listener) override {
    rtc::CritScope crit(&lock_);
    RTC_DCHECK(!listener_ || !listener);
    listener_ = listener;
    if (listener_) {
      parent_->RegisterAdapter(this);
    } else {
      parent_->UnregisterAdapter(this);
    }
  }

 private:
  BroadcastResourceListener* const parent_;
  const std::string name_;
  rtc::CriticalSection lock_;
  ResourceListener* listener_ RTC_GUARDED_BY(lock_) = nullptr;
};

BroadcastResourceListener::BroadcastResourceListener(
    rtc::scoped_refptr<Resource> source_resource)
    : source_resource_(source_resource) {
  RTC_DCHECK(source_resource_);
}

BroadcastResourceListener::~BroadcastResourceListener() {
  RTC_DCHECK(adapters_.empty());
}

rtc::scoped_refptr<Resource> BroadcastResourceListener::CreateAdapter() {
  // The adapter is responsible for registering and unregistering itself.
  return new rtc::RefCountedObject<AdapterResource>(
      this, source_resource_->Name() + "Adapter");
}

void BroadcastResourceListener::RegisterAdapter(
    rtc::scoped_refptr<AdapterResource> adapter) {
  rtc::CritScope crit(&lock_);
  RTC_DCHECK(std::find(adapters_.begin(), adapters_.end(), adapter) ==
             adapters_.end());
  adapters_.push_back(adapter);
}

void BroadcastResourceListener::UnregisterAdapter(
    rtc::scoped_refptr<AdapterResource> adapter) {
  rtc::CritScope crit(&lock_);
  auto it = std::find(adapters_.begin(), adapters_.end(), adapter);
  RTC_DCHECK(it != adapters_.end());
  adapters_.erase(it);
}

void BroadcastResourceListener::OnResourceUsageStateMeasured(
    rtc::scoped_refptr<Resource> resource,
    ResourceUsageState usage_state) {
  RTC_DCHECK_EQ(resource, source_resource_);
  std::vector<rtc::scoped_refptr<AdapterResource>> adapters_snapshot;
  {
    rtc::CritScope crit(&lock_);
    adapters_snapshot = adapters_;
  }
  // Not holding the lock here prevents a possible deadlock if measurements and
  // unregistering happens concurrently.
  for (const auto& adapter : adapters_snapshot) {
    adapter->OnResourceUsageStateMeasured(usage_state);
  }
}

std::vector<rtc::scoped_refptr<Resource>>
BroadcastResourceListener::GetAdaptersForTesting() {
  rtc::CritScope crit(&lock_);
  std::vector<rtc::scoped_refptr<Resource>> resources;
  for (const auto& adapter : adapters_) {
    resources.push_back(adapter);
  }
  return resources;
}

}  // namespace webrtc
