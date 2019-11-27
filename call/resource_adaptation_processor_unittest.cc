/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/resource_adaptation_processor.h"

#include "call/resource.h"
#include "call/test/fake_resources.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace adaptation {

TEST(ResourceAdaptationProcessorTest, HelloWorld) {
  FakeCpuResource cpu(0.75);
  printf("%s\n", cpu.ToString().c_str());
  EXPECT_EQ(ResourceUsageState::kStable, cpu.CurrentUsageState());
  cpu.set_usage(0.8);
  printf("%s\n", cpu.ToString().c_str());
  EXPECT_EQ(ResourceUsageState::kOveruse, cpu.CurrentUsageState());
  cpu.set_usage(0.6);
  printf("%s\n", cpu.ToString().c_str());
  EXPECT_EQ(ResourceUsageState::kUnderuse, cpu.CurrentUsageState());
}

}  // namespace adaptation
}  // namespace webrtc
