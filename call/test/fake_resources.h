/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_TEST_FAKE_RESOURCES_H_
#define CALL_TEST_FAKE_RESOURCES_H_

#include <string>

#include "call/resource.h"

namespace webrtc {
namespace adaptation {

// Fake resource representing CPU usage percent, with a setter.
// - [0.8, inf) triggers ResourceUsageState::kOveruse.
// - (0.6, 0.8) triggers ResourceUsageState::kStable.
// - (-inf, 0.6] triggers ResourceUsageState::kUnderuse.
// These numbers are arbitrary and don't necessarily represent how we want a
// real CPU resource to be treated.
class FakeCpuResource : public Resource {
 public:
  explicit FakeCpuResource(double usage);
  ~FakeCpuResource() override;

  void set_usage(double usage);

  std::string Name() const override;
  std::string UsageUnitsOfMeasurement() const override;
  double CurrentUsage() const override;
  ResourceUsageState CurrentUsageState() const override;

 private:
  double usage_;
};

}  // namespace adaptation
}  // namespace webrtc

#endif  // CALL_TEST_FAKE_RESOURCES_H_
