/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/test/fake_resources.h"

namespace webrtc {
namespace adaptation {

FakeCpuResource::FakeCpuResource(double usage) : usage_(usage) {}

FakeCpuResource::~FakeCpuResource() {}

void FakeCpuResource::set_usage(double usage) {
  usage_ = usage;
}

std::string FakeCpuResource::Name() const {
  return "CPU";
}

std::string FakeCpuResource::UsageUnitsOfMeasurement() const {
  return "%";
}

double FakeCpuResource::CurrentUsage() const {
  return usage_;
}

ResourceUsageState FakeCpuResource::CurrentUsageState() const {
  if (usage_ >= 0.8)
    return ResourceUsageState::kOveruse;
  else if (usage_ > 0.6)
    return ResourceUsageState::kStable;
  else
    return ResourceUsageState::kUnderuse;
}

}  // namespace adaptation
}  // namespace webrtc
