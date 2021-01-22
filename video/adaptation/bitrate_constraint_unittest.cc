/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/bitrate_constraint.h"

#include <limits>
#include <memory>
#include <vector>

#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

VideoCodec MakeVideoCodec(VideoCodecType codecType,
                          int widht_px,
                          int height_px,
                          std::vector<bool> active_flags) {
  VideoCodec video_codec;
  video_codec.codecType = codecType;
  if (codecType == kVideoCodecVP8) {
    video_codec.numberOfSimulcastStreams = active_flags.size();
    for (size_t layer_idx = 0; layer_idx < active_flags.size(); ++layer_idx) {
      video_codec.simulcastStream[layer_idx].active = active_flags[layer_idx];
      video_codec.simulcastStream[layer_idx].width = widht_px >> layer_idx;
      video_codec.simulcastStream[layer_idx].height = height_px >> layer_idx;
    }
  }
  return video_codec;
}

VideoEncoder::EncoderInfo MakeEncoderInfo(
    int widht_px,
    int height_px,
    std::vector<int> min_start_bitrate_bps) {
  VideoEncoder::EncoderInfo encoder_info;
  for (size_t layer_idx = 0; layer_idx < min_start_bitrate_bps.size();
       ++layer_idx) {
    int frame_size_px = (widht_px >> layer_idx) * (height_px >> layer_idx);
    VideoEncoder::ResolutionBitrateLimits bitrate_limits(
        frame_size_px, min_start_bitrate_bps[layer_idx], 0,
        std::numeric_limits<int>::max());
    encoder_info.resolution_bitrate_limits.push_back(bitrate_limits);
  }
  return encoder_info;
}
}  // namespace

TEST(BitrateConstraintTest,
     IsAdaptationUpAllowedReturnsTrueAtSinglecastIfBitrateIsEnough) {
  VideoCodec video_codec =
      MakeVideoCodec(kVideoCodecVP8, /*widht_px=*/640, /*height_px=*/360,
                     /*active_flags=*/{true});
  VideoEncoder::EncoderInfo encoder_info = MakeEncoderInfo(
      /*widht_px=*/1280, /*height_px=*/720,
      /*min_start_bitrate_bps=*/{1000 * 1000});
  EncoderSettings encoder_settings(encoder_info, VideoEncoderConfig(),
                                   video_codec);

  BitrateConstraint bitrate_constraint;
  bitrate_constraint.OnEncoderSettingsUpdated(encoder_settings);
  bitrate_constraint.OnEncoderTargetBitrateUpdated(1000 * 1000);

  VideoSourceRestrictions restrictions_before(
      /*max_pixels_per_frame=*/640 * 360, /*target_pixels_per_frame=*/640 * 360,
      /*max_frame_rate=*/30);
  VideoSourceRestrictions restrictions_after(
      /*max_pixels_per_frame=*/1280 * 720,
      /*target_pixels_per_frame=*/1280 * 720, /*max_frame_rate=*/30);
  EXPECT_TRUE(bitrate_constraint.IsAdaptationUpAllowed(
      VideoStreamInputState(), restrictions_before, restrictions_after));
}

TEST(BitrateConstraintTest,
     IsAdaptationUpAllowedReturnsFalseAtSinglecastIfBitrateIsNotEnough) {
  VideoCodec video_codec =
      MakeVideoCodec(kVideoCodecVP8, /*widht_px=*/640, /*height_px=*/360,
                     /*active_flags=*/{true});
  VideoEncoder::EncoderInfo encoder_info = MakeEncoderInfo(
      /*widht_px=*/1280, /*height_px=*/720,
      /*min_start_bitrate_bps=*/{1000 * 1000});
  EncoderSettings encoder_settings(encoder_info, VideoEncoderConfig(),
                                   video_codec);

  BitrateConstraint bitrate_constraint;
  bitrate_constraint.OnEncoderSettingsUpdated(encoder_settings);
  // One bit/s less then needed for 720p.
  bitrate_constraint.OnEncoderTargetBitrateUpdated(1000 * 1000 - 1);

  VideoSourceRestrictions restrictions_before(
      /*max_pixels_per_frame=*/640 * 360, /*target_pixels_per_frame=*/640 * 360,
      /*max_frame_rate=*/30);
  VideoSourceRestrictions restrictions_after(
      /*max_pixels_per_frame=*/1280 * 720,
      /*target_pixels_per_frame=*/1280 * 720, /*max_frame_rate=*/30);
  EXPECT_FALSE(bitrate_constraint.IsAdaptationUpAllowed(
      VideoStreamInputState(), restrictions_before, restrictions_after));
}

TEST(BitrateConstraintTest,
     IsAdaptationUpAllowedReturnsTrueAtSimucastIfBitrateIsNotEnough) {
  // 2 active layers.
  VideoCodec video_codec =
      MakeVideoCodec(kVideoCodecVP8, /*widht_px=*/640, /*height_px=*/360,
                     /*active_flags=*/{true, true});
  VideoEncoder::EncoderInfo encoder_info = MakeEncoderInfo(
      /*widht_px=*/1280, /*height_px=*/720,
      /*min_start_bitrate_bps=*/{1000 * 1000});
  EncoderSettings encoder_settings(encoder_info, VideoEncoderConfig(),
                                   video_codec);

  BitrateConstraint bitrate_constraint;
  bitrate_constraint.OnEncoderSettingsUpdated(encoder_settings);
  // One bit/s less then needed for 720p.
  bitrate_constraint.OnEncoderTargetBitrateUpdated(1000 * 1000 - 1);

  VideoSourceRestrictions restrictions_before(
      /*max_pixels_per_frame=*/640 * 360, /*target_pixels_per_frame=*/640 * 360,
      /*max_frame_rate=*/30);
  VideoSourceRestrictions restrictions_after(
      /*max_pixels_per_frame=*/1280 * 720,
      /*target_pixels_per_frame=*/1280 * 720, /*max_frame_rate=*/30);
  EXPECT_TRUE(bitrate_constraint.IsAdaptationUpAllowed(
      VideoStreamInputState(), restrictions_before, restrictions_after));
}

TEST(
    BitrateConstraintTest,
    IsAdaptationUpAllowedReturnsTrueAtSimucastWithOnlyLowestLayerActiveIfBitrateIsNotEnough) {
  VideoCodec video_codec =
      MakeVideoCodec(kVideoCodecVP8, /*widht_px=*/640, /*height_px=*/360,
                     /*active_flags=*/{true, false});
  VideoEncoder::EncoderInfo encoder_info = MakeEncoderInfo(
      /*widht_px=*/1280, /*height_px=*/720,
      /*min_start_bitrate_bps=*/{1000 * 1000});
  EncoderSettings encoder_settings(encoder_info, VideoEncoderConfig(),
                                   video_codec);

  BitrateConstraint bitrate_constraint;
  bitrate_constraint.OnEncoderSettingsUpdated(encoder_settings);
  // One bit/s less then needed for 720p.
  bitrate_constraint.OnEncoderTargetBitrateUpdated(1000 * 1000 - 1);

  VideoSourceRestrictions restrictions_before(
      /*max_pixels_per_frame=*/640 * 360, /*target_pixels_per_frame=*/640 * 360,
      /*max_frame_rate=*/30);
  VideoSourceRestrictions restrictions_after(
      /*max_pixels_per_frame=*/1280 * 720,
      /*target_pixels_per_frame=*/1280 * 720, /*max_frame_rate=*/30);
  EXPECT_TRUE(bitrate_constraint.IsAdaptationUpAllowed(
      VideoStreamInputState(), restrictions_before, restrictions_after));
}

TEST(
    BitrateConstraintTest,
    IsAdaptationUpAllowedReturnsTrueAtSimucastWithOnlyOneUpperLayerActiveIfBitrateIsEnough) {
  VideoCodec video_codec =
      MakeVideoCodec(kVideoCodecVP8, /*widht_px=*/640, /*height_px=*/360,
                     /*active_flags=*/{false, true});
  VideoEncoder::EncoderInfo encoder_info = MakeEncoderInfo(
      /*widht_px=*/1280, /*height_px=*/720,
      /*min_start_bitrate_bps=*/{1000 * 1000});
  EncoderSettings encoder_settings(encoder_info, VideoEncoderConfig(),
                                   video_codec);

  BitrateConstraint bitrate_constraint;
  bitrate_constraint.OnEncoderSettingsUpdated(encoder_settings);
  // One bit/s less then needed for 720p.
  bitrate_constraint.OnEncoderTargetBitrateUpdated(1000 * 1000);

  VideoSourceRestrictions restrictions_before(
      /*max_pixels_per_frame=*/640 * 360, /*target_pixels_per_frame=*/640 * 360,
      /*max_frame_rate=*/30);
  VideoSourceRestrictions restrictions_after(
      /*max_pixels_per_frame=*/1280 * 720,
      /*target_pixels_per_frame=*/1280 * 720, /*max_frame_rate=*/30);
  EXPECT_TRUE(bitrate_constraint.IsAdaptationUpAllowed(
      VideoStreamInputState(), restrictions_before, restrictions_after));
}

TEST(BitrateConstraintTest,
     IsAdaptationUpAllowedReturnsTrueIfBitrateLimitsAreNotSet) {
  VideoCodec video_codec =
      MakeVideoCodec(kVideoCodecVP8, /*widht_px=*/640, /*height_px=*/360,
                     /*active_flags=*/{true});
  EncoderSettings encoder_settings(VideoEncoder::EncoderInfo(),
                                   VideoEncoderConfig(), video_codec);

  BitrateConstraint bitrate_constraint;
  bitrate_constraint.OnEncoderSettingsUpdated(encoder_settings);
  bitrate_constraint.OnEncoderTargetBitrateUpdated(1000 * 1000);

  VideoSourceRestrictions restrictions_before(
      /*max_pixels_per_frame=*/640 * 360, /*target_pixels_per_frame=*/640 * 360,
      /*max_frame_rate=*/30);
  VideoSourceRestrictions restrictions_after(
      /*max_pixels_per_frame=*/1280 * 720,
      /*target_pixels_per_frame=*/1280 * 720, /*max_frame_rate=*/30);
  EXPECT_TRUE(bitrate_constraint.IsAdaptationUpAllowed(
      VideoStreamInputState(), restrictions_before, restrictions_after));
}

}  // namespace webrtc
