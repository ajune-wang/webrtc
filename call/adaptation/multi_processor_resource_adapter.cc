/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/multi_processor_resource_adapter.h"

#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

class MultiProcessorResourceAdapter::AdapterResource : public Resource {
 public:
  explicit AdapterResource(MultiProcessorResourceAdapter* parent)
      : parent_(parent) {
    RTC_DCHECK(parent_);
  }
  ~AdapterResource() override {
    printf("TODO(hbos): Un-register from Multi-swagger\n");
  }

  void OnParentDestroyed() { parent_ = nullptr; }

  void OnResourceUsageStateMeasured(ResourceUsageState usage_state) {
    // TODO(hbos): How's the thread safety?
    if (!listener_)
      return;
    listener_->OnResourceUsageStateMeasured(this, usage_state);
  }

  // Resource implementation.
  std::string Name() const override {
    return "MultiProcessorResourceAdapter::AdapterResource";
  }
  void SetResourceListener(ResourceListener* listener) override {
    listener_ = listener;
  }

 private:
  MultiProcessorResourceAdapter* parent_;
  ResourceListener* listener_ = nullptr;
};

MultiProcessorResourceAdapter::~MultiProcessorResourceAdapter() {}

rtc::scoped_refptr<Resource> MultiProcessorResourceAdapter::CreateAdapter() {
  rtc::scoped_refptr<AdapterResource> adapter =
      new rtc::RefCountedObject<AdapterResource>(this);
  adapters_.push_back(adapter);
  return adapter;
}

void MultiProcessorResourceAdapter::OnResourceUsageStateMeasured(
    rtc::scoped_refptr<Resource> resource,
    ResourceUsageState usage_state) {
  for (auto* adapter : adapters_) {
    adapter->OnResourceUsageStateMeasured(usage_state);
  }
}

}  // namespace webrtc
