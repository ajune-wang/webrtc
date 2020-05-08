/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"

#include <memory>
#include <vector>

#include "api/test/mock_video_encoder.h"
#include "api/video/i420_buffer.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

VideoCodec DefaultCodecSettings() {
  VideoCodec codec_settings;
  codec_settings.width = 1280;
  codec_settings.height = 720;
  codec_settings.maxFramerate = 30;
  return codec_settings;
}

VideoEncoder::Settings DefaultEncoderSettings() {
  VideoEncoder::Capabilities capabilities(/*loss_notification=*/false);
  VideoEncoder::Settings encoder_settings(capabilities, /*number_of_cores=*/1,
                                          /*max_payload_size=*/1200);
  return encoder_settings;
}

TEST(LibaomAv1EncoderTest, CanCreate) {
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  EXPECT_TRUE(encoder);
}

TEST(LibaomAv1EncoderTest, InitAndRelease) {
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  ASSERT_TRUE(encoder);

  VideoCodec codec_settings = DefaultCodecSettings();
  EXPECT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(encoder->Release(), WEBRTC_VIDEO_CODEC_OK);
}

TEST(LibaomAv1EncoderTest, DropFramesWhenRequestedByController) {
  class StubController : public FrameDependenciesController {
   public:
    FrameDependencyStructure DependencyStructure() const override { return {}; }
    std::vector<GenericFrameInfo> NextFrameConfig(bool /*restart*/) override {
      std::vector<GenericFrameInfo> no_layer_frames;
      return no_layer_frames;
    }
  };
  std::unique_ptr<VideoEncoder> encoder =
      CreateLibaomAv1Encoder(std::make_unique<StubController>());
  ASSERT_TRUE(encoder);
  VideoCodec codec_settings = DefaultCodecSettings();
  ASSERT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);
  MockEncodedImageCallback encoder_callback;
  ASSERT_EQ(encoder->RegisterEncodeCompleteCallback(&encoder_callback),
            WEBRTC_VIDEO_CODEC_OK);

  EXPECT_CALL(encoder_callback, OnEncodedImage).Times(0);
  EXPECT_CALL(
      encoder_callback,
      OnDroppedFrame(
          EncodedImageCallback::DropReason::kDroppedByMediaOptimizations));

  std::vector<VideoFrameType> frame_type = {VideoFrameType::kVideoFrameKey};
  EXPECT_EQ(
      encoder->Encode(VideoFrame::Builder()
                          .set_video_frame_buffer(I420Buffer::Create(320, 180))
                          .build(),
                      &frame_type),
      WEBRTC_VIDEO_CODEC_OK);
}

}  // namespace
}  // namespace webrtc
