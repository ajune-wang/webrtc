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

#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

// The AdapterResource redirects resource usage measurements from its parent to
// a single ResourceListener. When its listener is unregistered, the
// AdapterResource is unregistered from its parent - it cleans up after itself.
class BroadcastResourceListener::AdapterResource : public Resource {
 public:
  explicit AdapterResource(BroadcastResourceListener* parent)
      : name_("Adapter[" + parent->source_resource_->Name() + "]"),
        parent_(parent) {
    RTC_DCHECK(parent_);
  }
  ~AdapterResource() override {
    RTC_DCHECK(!parent_);
    RTC_DCHECK(!listener_);
  }

  // The parent is letting us know we have a usage neasurement.
  void OnResourceUsageStateMeasured(ResourceUsageState usage_state) {
    rtc::CritScope crit(&lock_);
    if (!listener_)
      return;
    listener_->OnResourceUsageStateMeasured(this, usage_state);
  }

  // The adapter was unregistered by the parent and is no longer receiving
  // measurements.
  void OnAdapterUnregistered() {
    rtc::CritScope crit(&lock_);
    parent_ = nullptr;
  }

  // Resource implementation.
  std::string Name() const override { return name_; }
  void SetResourceListener(ResourceListener* listener) override {
    rtc::CritScope crit(&lock_);
    RTC_DCHECK(!listener || parent_)
        << "Cannot register the adapter as a resource if the adapter does not "
        << "have a parent providing measurements.";
    RTC_DCHECK(!listener_ || !listener) << "A listener is already set.";
    listener_ = listener;
    if (!listener_) {
      // The adapter is no longer being listened to and no longer needs to
      // receive measurements from its parent.
      parent_->UnregisterAdapter(this);
      parent_ = nullptr;
    }
  }

 private:
  const std::string name_;
  // To avoid deadlock...
  // - It is NOT OK to hold parent's lock while acquiring adapter's lock.
  // - It is OK to hold adapter's lock while acquiring parent's lock.
  rtc::CriticalSection lock_;
  BroadcastResourceListener* parent_ RTC_GUARDED_BY(lock_);
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
  rtc::scoped_refptr<AdapterResource> adapter =
      new rtc::RefCountedObject<AdapterResource>(this);
  rtc::CritScope crit(&lock_);
  adapters_.push_back(adapter);
  return adapter;
}

void BroadcastResourceListener::UnregisterAdapter(AdapterResource* adapter) {
  rtc::CritScope crit(&lock_);
  auto it = std::find(adapters_.begin(), adapters_.end(), adapter);
  if (it == adapters_.end()) {
    // In the rare event that UnregisterAllAdapters() and UnregisterAdapter()
    // are racing, |adapter| may already have been unregistered.
    return;
  }
  adapters_.erase(it);
}

void BroadcastResourceListener::UnregisterAllAdapters() {
  // Keep unregistered adapters alive until OnAdapterUnregistered() is called,
  // which is called while not holding the lock to avoid possible deadlock.
  std::vector<rtc::scoped_refptr<AdapterResource>> unregistered_adapters;
  {
    rtc::CritScope crit(&lock_);
    for (auto* adapter : adapters_) {
      unregistered_adapters.push_back(adapter);
    }
    adapters_.clear();
  }
  for (const auto& unregistered_adapter : unregistered_adapters) {
    unregistered_adapter->OnAdapterUnregistered();
  }
}

void BroadcastResourceListener::OnResourceUsageStateMeasured(
    rtc::scoped_refptr<Resource> resource,
    ResourceUsageState usage_state) {
  RTC_DCHECK_EQ(resource, source_resource_);
  rtc::CritScope crit(&lock_);
  for (auto* adapter : adapters_) {
    adapter->OnResourceUsageStateMeasured(usage_state);
  }
}

}  // namespace webrtc
