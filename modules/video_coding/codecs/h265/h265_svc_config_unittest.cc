/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/h265/h265_svc_config.h"

#include "api/video_codecs/video_codec.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr int kDontCare = 0;

VideoCodec GetDefaultVideoCodec() {
  VideoCodec video_codec;
  video_codec.codecType = kVideoCodecH265;
  video_codec.width = 1280;
  video_codec.height = 720;
  return video_codec;
}

TEST(H265SvcConfigTest, TreatsEmptyAsL1T1) {
  VideoCodec video_codec = GetDefaultVideoCodec();

  SetH265SvcConfig(video_codec, /*num_temporal_layers=*/kDontCare);
  EXPECT_TRUE(video_codec.spatialLayers[0].active);
  EXPECT_EQ(video_codec.spatialLayers[0].numberOfTemporalLayers, 1);
  EXPECT_FALSE(video_codec.spatialLayers[1].active);
}

TEST(H265SvcConfigTest, ScalabilityModeFromNumberOfTemporalLayers) {
  VideoCodec video_codec = GetDefaultVideoCodec();

  SetH265SvcConfig(video_codec, /*num_temporal_layers=*/3);
  EXPECT_EQ(video_codec.spatialLayers[0].numberOfTemporalLayers, 3);
  EXPECT_FALSE(video_codec.spatialLayers[1].active);
}

TEST(H265SvcConfigTest, CopiesFramrate) {
  VideoCodec video_codec = GetDefaultVideoCodec();
  video_codec.SetScalabilityMode(ScalabilityMode::kL1T2);
  video_codec.maxFramerate = 27;

  SetH265SvcConfig(video_codec, /*num_temporal_layers=*/kDontCare);
  EXPECT_EQ(video_codec.spatialLayers[0].maxFramerate, 27);
  EXPECT_FALSE(video_codec.spatialLayers[1].active);
}

TEST(H265SvcConfigTest, SetsNumberOfTemporalLayers) {
  VideoCodec video_codec = GetDefaultVideoCodec();
  video_codec.SetScalabilityMode(ScalabilityMode::kL1T3);

  SetH265SvcConfig(video_codec, /*num_temporal_layers=*/kDontCare);
  EXPECT_EQ(video_codec.spatialLayers[0].numberOfTemporalLayers, 3);
  EXPECT_FALSE(video_codec.spatialLayers[1].active);
}

TEST(H265SvcConfigTest, CopiesMinMaxBitrateForSingleSpatialLayer) {
  VideoCodec video_codec;
  video_codec.codecType = kVideoCodecH265;
  video_codec.SetScalabilityMode(ScalabilityMode::kL1T3);
  video_codec.minBitrate = 100;
  video_codec.maxBitrate = 500;

  SetH265SvcConfig(video_codec, /*num_temporal_layers=*/kDontCare);
  EXPECT_EQ(video_codec.spatialLayers[0].minBitrate, 100u);
  EXPECT_EQ(video_codec.spatialLayers[0].maxBitrate, 500u);
  EXPECT_LE(video_codec.spatialLayers[0].minBitrate,
            video_codec.spatialLayers[0].targetBitrate);
  EXPECT_LE(video_codec.spatialLayers[0].targetBitrate,
            video_codec.spatialLayers[0].maxBitrate);
  EXPECT_FALSE(video_codec.spatialLayers[1].active);
}

}  // namespace
}  // namespace webrtc
