/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/resource.h"

#include "rtc_base/checks.h"

namespace webrtc {

ResourceUsageListener::~ResourceUsageListener() {}

Resource::Resource() : usage_state_(ResourceUsageState::kStable) {}

Resource::~Resource() {}

void Resource::RegisterListener(ResourceUsageListener* listener) {
  RTC_DCHECK(listener);
  listeners_.push_back(listener);
}

ResourceUsageState Resource::usage_state() const {
  return usage_state_;
}

void Resource::OnResourceUsageStateMeasured(ResourceUsageState usage_state) {
  usage_state_ = usage_state;
  for (auto* listener : listeners_) {
    listener->OnResourceUsageStateMeasured(*this, usage_state_);
  }
}

}  // namespace webrtc
