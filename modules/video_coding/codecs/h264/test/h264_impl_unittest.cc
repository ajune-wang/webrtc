/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>

#include <memory>

#include "absl/types/optional.h"
#include "api/test/create_frame_generator.h"
#include "api/video/color_space.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/test/video_codec_unittest.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/gtest.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace {
const VideoEncoder::Capabilities kCapabilities(false);
const VideoEncoder::Settings kSettings(kCapabilities,
                                       /*number_of_cores=*/1,
                                       /*max_payload_size=*/0);
}  // namespace

class TestH264Impl : public VideoCodecUnitTest {
 protected:
  std::unique_ptr<VideoEncoder> CreateEncoder() override {
    return H264Encoder::Create(cricket::VideoCodec(cricket::kH264CodecName));
  }

  std::unique_ptr<VideoDecoder> CreateDecoder() override {
    return H264Decoder::Create();
  }

  void ModifyCodecSettings(VideoCodec* codec_settings) override {
    webrtc::test::CodecSettings(kVideoCodecH264, codec_settings);
  }

  void EncodeAndWaitForFrame(const VideoFrame& input_frame,
                             EncodedImage* encoded_frame,
                             CodecSpecificInfo* codec_specific_info,
                             bool keyframe = false) {
    std::vector<VideoFrameType> frame_types;
    if (keyframe) {
      frame_types.emplace_back(VideoFrameType::kVideoFrameKey);
    } else {
      frame_types.emplace_back(VideoFrameType::kVideoFrameDelta);
    }
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(input_frame, &frame_types));
    ASSERT_TRUE(WaitForEncodedFrame(encoded_frame, codec_specific_info));
    EXPECT_EQ(kVideoCodecH264, codec_specific_info->codecType);
    EXPECT_EQ(0, encoded_frame->SpatialIndex());
  }

  void EncodeAndExpectFrameWith(const VideoFrame& input_frame,
                                uint8_t temporal_idx,
                                bool keyframe = false) {
    EncodedImage encoded_frame;
    CodecSpecificInfo codec_specific_info;
    EncodeAndWaitForFrame(input_frame, &encoded_frame, &codec_specific_info,
                          keyframe);
    EXPECT_EQ(temporal_idx,
              codec_specific_info.codecSpecific.H264.temporal_idx);
  }
};

#ifdef WEBRTC_USE_H264
#define MAYBE_EncodeDecode EncodeDecode
#define MAYBE_DecodedQpEqualsEncodedQp DecodedQpEqualsEncodedQp
#define MAYBE_EncoderWith2TemporalLayers EncoderWith2TemporalLayers
#define MAYBE_ReduceTemporalLayers ReduceTemporalLayers
#else
#define MAYBE_EncodeDecode DISABLED_EncodeDecode
#define MAYBE_DecodedQpEqualsEncodedQp DISABLED_DecodedQpEqualsEncodedQp
#define MAYBE_EncoderWith2TemporalLayers DISABLED_EncoderWith2TemporalLayers
#define MAYBE_ReduceTemporalLayers DISABLED_ReduceTemporalLayers
#endif

TEST_F(TestH264Impl, MAYBE_EncodeDecode) {
  VideoFrame input_frame = NextInputFrame();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Encode(input_frame, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  // First frame should be a key frame.
  encoded_frame._frameType = VideoFrameType::kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Decode(encoded_frame, false, 0));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  EXPECT_GT(I420PSNR(&input_frame, decoded_frame.get()), 36);

  const ColorSpace color_space = *decoded_frame->color_space();
  EXPECT_EQ(ColorSpace::PrimaryID::kUnspecified, color_space.primaries());
  EXPECT_EQ(ColorSpace::TransferID::kUnspecified, color_space.transfer());
  EXPECT_EQ(ColorSpace::MatrixID::kUnspecified, color_space.matrix());
  EXPECT_EQ(ColorSpace::RangeID::kInvalid, color_space.range());
  EXPECT_EQ(ColorSpace::ChromaSiting::kUnspecified,
            color_space.chroma_siting_horizontal());
  EXPECT_EQ(ColorSpace::ChromaSiting::kUnspecified,
            color_space.chroma_siting_vertical());
}

TEST_F(TestH264Impl, MAYBE_DecodedQpEqualsEncodedQp) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Encode(NextInputFrame(), nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  // First frame should be a key frame.
  encoded_frame._frameType = VideoFrameType::kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Decode(encoded_frame, false, 0));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  ASSERT_TRUE(decoded_qp);
  EXPECT_EQ(encoded_frame.qp_, *decoded_qp);
}

TEST_F(TestH264Impl, MAYBE_EncoderWith2TemporalLayers) {
  codec_settings_.simulcastStream[0].SetNumberOfTemporalLayers(2);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  // Temporal layer 0.
  EncodeAndExpectFrameWith(NextInputFrame(), 0);
  // Temporal layer 1.
  EncodeAndExpectFrameWith(NextInputFrame(), 1);
  // Temporal layer 0.
  EncodeAndExpectFrameWith(NextInputFrame(), 0);
  // Temporal layer 1.
  EncodeAndExpectFrameWith(NextInputFrame(), 1);
}

TEST_F(TestH264Impl, MAYBE_ReduceTemporalLayers) {
  const int kScreenWidth = 1280;
  const int kScreenHeight = 720;

  codec_settings_.width = kScreenWidth;
  codec_settings_.height = kScreenHeight;

  codec_settings_.simulcastStream[0].SetNumberOfTemporalLayers(2);
  codec_settings_.maxBitrate = 2500;

  input_frame_generator_ = test::CreateSquareFrameGenerator(
      codec_settings_.width, codec_settings_.height,
      test::FrameGeneratorInterface::OutputType::kI420,
      /* num_squares = */ absl::optional<int>(300));

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, kSettings));

  // Temporal layer 0.
  EncodeAndExpectFrameWith(NextInputFrame(), 0);
  // Temporal layer 1.
  EncodeAndExpectFrameWith(NextInputFrame(), 1);
  // Temporal layer 0.
  EncodeAndExpectFrameWith(NextInputFrame(), 0);

  VideoBitrateAllocation bitrate_allocation;
  // Bitrate only enough for TL0.
  bitrate_allocation.SetBitrate(0, 0, 200'000);
  encoder_->SetRates(
      VideoEncoder::RateControlParameters(bitrate_allocation, 30.0));

  // Temporal layer 1 should not occur.
  EncodeAndExpectFrameWith(NextInputFrame(), 0);
  EncodeAndExpectFrameWith(NextInputFrame(), 0);
  EncodeAndExpectFrameWith(NextInputFrame(), 0);

  // Restore bitrate
  VideoBitrateAllocation bitrate_allocation2;
  bitrate_allocation2.SetBitrate(0, 0, 180'000);
  bitrate_allocation2.SetBitrate(0, 1, 120'000);
  encoder_->SetRates(
      VideoEncoder::RateControlParameters(bitrate_allocation2, 30.0));

  // Temporal layer 0.
  EncodeAndExpectFrameWith(NextInputFrame(), 0);
  // Temporal layer 1.
  EncodeAndExpectFrameWith(NextInputFrame(), 1);
  // Temporal layer 0.
  EncodeAndExpectFrameWith(NextInputFrame(), 0);
}

}  // namespace webrtc
