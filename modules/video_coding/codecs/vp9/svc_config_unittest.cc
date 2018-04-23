/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "modules/video_coding/codecs/vp9/svc_config.h"
#include "test/gtest.h"

namespace webrtc {
TEST(SvcRateAllocatorTest, NumSpatialLayers) {
  const size_t max_num_spatial_layers = 6;
  const size_t expected_num_spatial_layers = 2;

  std::vector<SpatialLayer> spatial_layers =
      GetSvcConfig(expected_num_spatial_layers * kMinVp9SpatialLayerWidth,
                   expected_num_spatial_layers * kMinVp9SpatialLayerHeight,
                   max_num_spatial_layers, 1);

  RTC_CHECK_EQ(spatial_layers.size(), expected_num_spatial_layers);
}

TEST(SvcRateAllocatorTest, BitrateThresholds) {
  std::vector<SpatialLayer> spatial_layers = GetSvcConfig(
      3 * kMinVp9SpatialLayerWidth, 3 * kMinVp9SpatialLayerHeight, 3, 1);
  RTC_CHECK_EQ(spatial_layers.size(), 3);

  for (const SpatialLayer& layer : spatial_layers) {
    EXPECT_LE(layer.minBitrate, layer.maxBitrate);
    EXPECT_LE(layer.minBitrate, layer.targetBitrate);
    EXPECT_LE(layer.targetBitrate, layer.maxBitrate);
  }
}
}  // namespace webrtc
