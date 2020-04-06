/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_bitrate_allocation.h"

#include <vector>

#include "absl/types/optional.h"
#include "test/gtest.h"

namespace webrtc {

TEST(VideoBitrateAllocation, SetBitrateWithInvalidTemporalId) {
  VideoBitrateAllocation bitrate;
  EXPECT_DEBUG_DEATH(EXPECT_FALSE(bitrate.SetBitrate(0, 7, 10000)), "");
}

// Video parsing of packets allow 8 temporal ids for Vp9.
// Dont crash in release builds if such layer is queried.
TEST(VideoBitrateAllocation, GetBitrateWithInvalidTemporalId) {
  VideoBitrateAllocation bitrate;
  EXPECT_DEBUG_DEATH(EXPECT_EQ(bitrate.GetBitrate(1, 7), 0u), "");
}

TEST(VideoBitrateAllocation, SimulcastTargetBitrate) {
  VideoBitrateAllocation bitrate;
  bitrate.SetBitrate(0, 0, 10000);
  bitrate.SetBitrate(0, 1, 20000);
  bitrate.SetBitrate(1, 0, 40000);
  bitrate.SetBitrate(1, 1, 80000);

  VideoBitrateAllocation layer0_bitrate;
  layer0_bitrate.SetBitrate(0, 0, 10000);
  layer0_bitrate.SetBitrate(0, 1, 20000);

  VideoBitrateAllocation layer1_bitrate;
  layer1_bitrate.SetBitrate(0, 0, 40000);
  layer1_bitrate.SetBitrate(0, 1, 80000);

  std::vector<absl::optional<VideoBitrateAllocation>> layer_allocations =
      bitrate.GetSimulcastAllocations();

  EXPECT_EQ(layer0_bitrate, layer_allocations[0]);
  EXPECT_EQ(layer1_bitrate, layer_allocations[1]);
}

TEST(VideoBitrateAllocation, SimulcastTargetBitrateWithInactiveStream) {
  // Create bitrate allocation with bitrate only for the first and third stream.
  VideoBitrateAllocation bitrate;
  bitrate.SetBitrate(0, 0, 10000);
  bitrate.SetBitrate(0, 1, 20000);
  bitrate.SetBitrate(2, 0, 40000);
  bitrate.SetBitrate(2, 1, 80000);

  VideoBitrateAllocation layer0_bitrate;
  layer0_bitrate.SetBitrate(0, 0, 10000);
  layer0_bitrate.SetBitrate(0, 1, 20000);

  VideoBitrateAllocation layer2_bitrate;
  layer2_bitrate.SetBitrate(0, 0, 40000);
  layer2_bitrate.SetBitrate(0, 1, 80000);

  std::vector<absl::optional<VideoBitrateAllocation>> layer_allocations =
      bitrate.GetSimulcastAllocations();

  EXPECT_EQ(layer0_bitrate, layer_allocations[0]);
  EXPECT_FALSE(layer_allocations[1]);
  EXPECT_EQ(layer2_bitrate, layer_allocations[2]);
}
}  // namespace webrtc
