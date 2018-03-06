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

#include "modules/video_coding/codecs/vp9/svc_layering_config.h"
#include "modules/video_coding/codecs/vp9/svc_rate_allocator.h"
#include "test/gtest.h"

namespace webrtc {

class SvcRateAllocatorTest : public testing::Test {
 public:
  VideoCodec Configure(size_t width,
                       size_t height,
                       size_t num_spatial_layers,
                       size_t num_temporal_layers) const {
    VideoCodec codec;
    codec.width = width;
    codec.height = height;
    codec.codecType = kVideoCodecVP9;

    std::vector<SpatialLayer> spatial_layers = ConfigureSvcLayering(
        width, height, num_spatial_layers, num_temporal_layers);
    RTC_CHECK_LE(spatial_layers.size(), kMaxSpatialLayers);

    codec.VP9()->numberOfSpatialLayers =
        std::min<unsigned char>(num_spatial_layers, spatial_layers.size());
    codec.VP9()->numberOfTemporalLayers = std::min<unsigned char>(
        num_temporal_layers, spatial_layers.back().numberOfTemporalLayers);

    for (size_t sl_idx = 0; sl_idx < spatial_layers.size(); ++sl_idx) {
      codec.spatialLayers[sl_idx] = spatial_layers[sl_idx];
    }

    return codec;
  }
};

TEST_F(SvcRateAllocatorTest, SingleLayerFor320x180Input) {
  VideoCodec codec = Configure(320, 180, 3, 3);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  BitrateAllocation allocation = allocator.GetAllocation(1000 * 1000, 30);

  EXPECT_GT(allocation.GetBitrate(0, 2), 0u);
  EXPECT_EQ(allocation.GetBitrate(1, 2), 0u);
}

TEST_F(SvcRateAllocatorTest, TwoLayersFor640x360Input) {
  VideoCodec codec = Configure(640, 360, 3, 3);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  BitrateAllocation allocation = allocator.GetAllocation(1000 * 1000, 30);

  EXPECT_GT(allocation.GetBitrate(0, 2), 0u);
  EXPECT_GT(allocation.GetBitrate(1, 2), 0u);
  EXPECT_EQ(allocation.GetBitrate(2, 2), 0u);
}

TEST_F(SvcRateAllocatorTest, ThreeLayersFor1280x720Input) {
  VideoCodec codec = Configure(1280, 720, 3, 3);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  BitrateAllocation allocation = allocator.GetAllocation(1000 * 1000, 30);

  EXPECT_GT(allocation.GetBitrate(0, 2), 0u);
  EXPECT_GT(allocation.GetBitrate(1, 2), 0u);
  EXPECT_GT(allocation.GetBitrate(2, 2), 0u);
}

TEST_F(SvcRateAllocatorTest, Disable640x360Layer) {
  VideoCodec codec = Configure(1280, 720, 3, 3);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  const SpatialLayer* layers = codec.spatialLayers;

  size_t min_bitrate_for_640x360_layers_kbps =
      layers[0].minBitrate + layers[1].minBitrate;

  BitrateAllocation allocation = allocator.GetAllocation(
      min_bitrate_for_640x360_layers_kbps * 1000 - 1, 30);

  EXPECT_GT(allocation.GetBitrate(0, 2), 0u);
  EXPECT_EQ(allocation.GetBitrate(1, 2), 0u);
}

TEST_F(SvcRateAllocatorTest, Disable1280x720Layer) {
  VideoCodec codec = Configure(1280, 720, 3, 3);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  const SpatialLayer* layers = codec.spatialLayers;

  size_t min_bitrate_for_1280x720_layers_kbps =
      layers[0].minBitrate + layers[1].minBitrate + layers[2].minBitrate;

  BitrateAllocation allocation = allocator.GetAllocation(
      min_bitrate_for_1280x720_layers_kbps * 1000 - 1, 30);

  EXPECT_GT(allocation.GetBitrate(0, 2), 0u);
  EXPECT_GT(allocation.GetBitrate(1, 2), 0u);
  EXPECT_EQ(allocation.GetBitrate(2, 2), 0u);
}

}  // namespace webrtc
