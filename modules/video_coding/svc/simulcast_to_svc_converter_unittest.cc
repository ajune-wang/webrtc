/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/svc/simulcast_to_svc_converter.h"

#include <cstddef>
#include <vector>

#include "test/gtest.h"

namespace webrtc {

TEST(SimulcastToSvc, ConvertsConfig) {
  VideoCodec codec;
  codec.codecType = kVideoCodecVP9;
  codec.SetScalabilityMode(ScalabilityMode::kL1T3);
  codec.width = 1280;
  codec.height = 720;
  codec.minBitrate = 10;
  codec.maxBitrate = 2500;
  codec.numberOfSimulcastStreams = 3;
  codec.VP9()->numberOfSpatialLayers = 1;
  codec.VP9()->interLayerPred = InterLayerPredMode::kOff;
  codec.simulcastStream[0] = {.width = 320,
                              .height = 180,
                              .maxFramerate = 30,
                              .numberOfTemporalLayers = 3,
                              .maxBitrate = 100,
                              .targetBitrate = 70,
                              .minBitrate = 50,
                              .qpMax = 150,
                              .active = true};
  codec.simulcastStream[1] = {.width = 640,
                              .height = 360,
                              .maxFramerate = 30,
                              .numberOfTemporalLayers = 3,
                              .maxBitrate = 250,
                              .targetBitrate = 150,
                              .minBitrate = 100,
                              .qpMax = 150,
                              .active = true};
  codec.simulcastStream[2] = {.width = 12800,
                              .height = 720,
                              .maxFramerate = 30,
                              .numberOfTemporalLayers = 3,
                              .maxBitrate = 1500,
                              .targetBitrate = 1200,
                              .minBitrate = 800,
                              .qpMax = 150,
                              .active = true};
  VideoCodec result = codec;

  SimulcastToSvcConverter converter(codec);
  result = converter.GetConfig();

  EXPECT_EQ(result.numberOfSimulcastStreams, 1);
  EXPECT_EQ(result.spatialLayers[0], codec.simulcastStream[0]);
  EXPECT_EQ(result.spatialLayers[1], codec.simulcastStream[1]);
  EXPECT_EQ(result.spatialLayers[2], codec.simulcastStream[2]);
  EXPECT_EQ(result.VP9()->numberOfTemporalLayers, 3);
  EXPECT_EQ(result.VP9()->numberOfSpatialLayers, 3);
  EXPECT_EQ(result.VP9()->interLayerPred, InterLayerPredMode::kOff);
}

TEST(SimulcastToSvc, ConvertsEncodedImage) {
  VideoCodec codec;
  codec.codecType = kVideoCodecVP9;
  codec.SetScalabilityMode(ScalabilityMode::kL1T3);
  codec.width = 1280;
  codec.height = 720;
  codec.minBitrate = 10;
  codec.maxBitrate = 2500;
  codec.numberOfSimulcastStreams = 3;
  codec.VP9()->numberOfSpatialLayers = 1;
  codec.VP9()->interLayerPred = InterLayerPredMode::kOff;
  codec.simulcastStream[0] = {.width = 320,
                              .height = 180,
                              .maxFramerate = 30,
                              .numberOfTemporalLayers = 3,
                              .maxBitrate = 100,
                              .targetBitrate = 70,
                              .minBitrate = 50,
                              .qpMax = 150,
                              .active = true};
  codec.simulcastStream[1] = {.width = 640,
                              .height = 360,
                              .maxFramerate = 30,
                              .numberOfTemporalLayers = 3,
                              .maxBitrate = 250,
                              .targetBitrate = 150,
                              .minBitrate = 100,
                              .qpMax = 150,
                              .active = true};
  codec.simulcastStream[2] = {.width = 12800,
                              .height = 720,
                              .maxFramerate = 30,
                              .numberOfTemporalLayers = 3,
                              .maxBitrate = 1500,
                              .targetBitrate = 1200,
                              .minBitrate = 800,
                              .qpMax = 150,
                              .active = true};

  SimulcastToSvcConverter converter(codec);

  EncodedImage image;
  image.SetRtpTimestamp(123);
  image.SetSpatialIndex(1);
  image.SetTemporalIndex(0);
  image._encodedWidth = 640;
  image._encodedHeight = 360;

  CodecSpecificInfo codec_specific;
  codec_specific.codecType = kVideoCodecVP9;
  codec_specific.end_of_picture = false;
  codec_specific.codecSpecific.VP9.num_spatial_layers = 3;
  codec_specific.codecSpecific.VP9.first_active_layer = 0;
  codec_specific.scalability_mode = ScalabilityMode::kS3T3;

  converter.EncodeStarted(/*force_keyframe =*/true);
  converter.ConvertFrame(image, codec_specific);

  EXPECT_EQ(image.SpatialIndex(), absl::nullopt);
  EXPECT_EQ(image.SimulcastIndex().value_or(-1), 1);
  EXPECT_EQ(image.TemporalIndex().value_or(-1), 0);

  EXPECT_EQ(codec_specific.end_of_picture, true);
  EXPECT_EQ(codec_specific.scalability_mode, ScalabilityMode::kL1T3);
}

}  // namespace webrtc
