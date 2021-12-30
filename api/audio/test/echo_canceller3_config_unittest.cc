/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio/echo_canceller3_config.h"

#include "test/gtest.h"

namespace webrtc {

TEST(EchoCanceller3Config, ValidatedConfigsAreValid) {
  EchoCanceller3Config config;
  config.delay.down_sampling_factor = 983;
  EXPECT_FALSE(EchoCanceller3Config::Validate(&config));
  EXPECT_TRUE(EchoCanceller3Config::Validate(&config));
}

}  // namespace webrtc
