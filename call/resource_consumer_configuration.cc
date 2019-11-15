/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/resource_consumer_configuration.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace adaptation {

ResourceConsumerConfiguration::ResourceConsumerConfiguration(std::string name)
    : name_(std::move(name)) {
  RTC_DCHECK(!name_.empty());
}

ResourceConsumerConfiguration::~ResourceConsumerConfiguration() {}

std::string ResourceConsumerConfiguration::name() const {
  return name_;
}

double ResourceConsumerConfiguration::ResourceImpactFactor(Resource* resource)
    const {
  auto it = impact_factor_by_resource_.find(resource);
  if (it == impact_factor_by_resource_.end())
    return 1.0;
  return it->second;
}

void ResourceConsumerConfiguration::SetResourceImpactFactor(
    Resource* resource, double factor) {
  impact_factor_by_resource_.insert(std::make_pair(resource, factor));
}

}  // namespace adaptation
}  // namespace webrtc
