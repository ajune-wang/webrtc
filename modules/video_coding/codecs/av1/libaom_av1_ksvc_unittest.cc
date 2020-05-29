/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/test/create_frame_generator.h"
#include "api/test/frame_generator_interface.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/av1/libaom_av1_decoder.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller_no_layering.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ContainerEq;
using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::SizeIs;
using ::testing::Truly;
using ::testing::Values;

// Use small resolution for this test to make it faster.
constexpr int kWidth = 320;
constexpr int kHeight = 180;
constexpr int kFramerate = 30;
constexpr int kRtpTicksPerSecond = 90000;

class TestAv1Encoder {
 public:
  TestAv1Encoder() : encoder_(CreateLibaomAv1Encoder()) { InitEncoder(); }
  explicit TestAv1Encoder(
      std::unique_ptr<ScalableVideoController> svc_controller)
      : encoder_(CreateLibaomAv1Encoder(std::move(svc_controller))) {
    InitEncoder();
  }
  // This class requires pointer stability and thus not copyable nor movable.
  TestAv1Encoder(const TestAv1Encoder&) = delete;
  TestAv1Encoder& operator=(const TestAv1Encoder&) = delete;

  void EncodeAndAppend(const VideoFrame& frame,
                       std::vector<EncodedImage>& encoded) {
    callback_.SetEncodeStorage(&encoded);
    std::vector<VideoFrameType> frame_types = {
        VideoFrameType::kVideoFrameDelta};
    EXPECT_EQ(encoder_->Encode(frame, &frame_types), WEBRTC_VIDEO_CODEC_OK);
    // Prefer to crash checking nullptr rather than writing to random memory.
    callback_.SetEncodeStorage(nullptr);
  }

 private:
  class EncoderCallback : public EncodedImageCallback {
   public:
    void SetEncodeStorage(std::vector<EncodedImage>* storage) {
      storage_ = storage;
    }

   private:
    Result OnEncodedImage(
        const EncodedImage& encoded_image,
        const CodecSpecificInfo* codec_specific_info,
        const RTPFragmentationHeader* /*fragmentation*/) override {
      RTC_CHECK(storage_);
      storage_->push_back(encoded_image);
      return Result(Result::Error::OK);
    }

    std::vector<EncodedImage>* storage_ = nullptr;
  };

  void InitEncoder() {
    RTC_CHECK(encoder_);
    VideoCodec codec_settings;
    codec_settings.width = kWidth;
    codec_settings.height = kHeight;
    codec_settings.maxFramerate = kFramerate;
    VideoEncoder::Settings encoder_settings(
        VideoEncoder::Capabilities(/*loss_notification=*/false),
        /*number_of_cores=*/1, /*max_payload_size=*/1200);
    EXPECT_EQ(encoder_->InitEncode(&codec_settings, encoder_settings),
              WEBRTC_VIDEO_CODEC_OK);
    EXPECT_EQ(encoder_->RegisterEncodeCompleteCallback(&callback_),
              WEBRTC_VIDEO_CODEC_OK);
  }

  EncoderCallback callback_;
  std::unique_ptr<VideoEncoder> encoder_;
};

class TestAv1Decoder {
 public:
  TestAv1Decoder() : decoder_(CreateLibaomAv1Decoder()) {
    RTC_CHECK(decoder_);
    EXPECT_EQ(decoder_->InitDecode(/*codec_settings=*/nullptr,
                                   /*number_of_cores=*/1),
              WEBRTC_VIDEO_CODEC_OK);
    EXPECT_EQ(decoder_->RegisterDecodeCompleteCallback(&callback_),
              WEBRTC_VIDEO_CODEC_OK);
  }
  // This class requires pointer stability and thus not copyable nor movable.
  TestAv1Decoder(const TestAv1Decoder&) = delete;
  TestAv1Decoder& operator=(const TestAv1Decoder&) = delete;

  int32_t Decode(const EncodedImage& image) {
    return decoder_->Decode(image, /*missing_frames=*/false,
                            /*render_time_ms=*/image.capture_time_ms_);
  }

 private:
  // Decoder callback that only counts how many times it was called.
  // While it is tempting to replace it with a simple mock, that one requires
  // to set expectation on number of calls in advance. Tests below unsure about
  // expected number of calls until after calls are done.
  class DecoderCallback : public DecodedImageCallback {
   private:
    int32_t Decoded(VideoFrame& /*decoded_image*/) override { return 0; }
  };

  DecoderCallback callback_;
  const std::unique_ptr<VideoDecoder> decoder_;
};

class VideoFrameGenerator {
 public:
  VideoFrame NextFrame() {
    return VideoFrame::Builder()
        .set_video_frame_buffer(frame_buffer_generator_->NextFrame().buffer)
        .set_timestamp_rtp(timestamp_ += kRtpTicksPerSecond / kFramerate)
        .build();
  }

 private:
  uint32_t timestamp_ = 1000;
  std::unique_ptr<test::FrameGeneratorInterface> frame_buffer_generator_ =
      test::CreateSquareFrameGenerator(
          kWidth,
          kHeight,
          test::FrameGeneratorInterface::OutputType::kI420,
          absl::nullopt);
};

class ScalableVideoL2T1Ksvc : public ScalableVideoController {
 public:
  ScalableVideoL2T1Ksvc(int s1_delta_id) : s1_delta_id_(s1_delta_id) {}
  ~ScalableVideoL2T1Ksvc() override = default;

  StreamLayersConfig StreamConfig() const override {
    StreamLayersConfig config;
    config.num_spatial_layers = 2;
    config.num_temporal_layers = 1;
    return config;
  }
  std::vector<LayerFrameConfig> NextFrameConfig(bool /*restart*/) override {
    std::vector<LayerFrameConfig> result(2);
    if (keyframe_) {
      result[0].spatial_id = 0;
      result[0].is_keyframe = true;
      result[0].id = 0;

      result[1].spatial_id = 1;
      result[1].is_keyframe = false;
      result[1].id = 1;
      keyframe_ = false;
    } else {
      result[0].spatial_id = 0;
      result[0].is_keyframe = false;
      result[0].id = 2;

      result[1].spatial_id = 1;
      result[1].is_keyframe = false;
      result[1].id = s1_delta_id_;
    }
    return result;
  }

 private:
  FrameDependencyStructure DependencyStructure() const override { return {}; }
  absl::optional<GenericFrameInfo> OnEncodeDone(
      LayerFrameConfig config) override {
    return {};
  }

  bool keyframe_ = true;
  const int s1_delta_id_;
};

class LibaomAv1KSvcTest : public ::testing::TestWithParam<int> {};

TEST_P(LibaomAv1KSvcTest, DecodeS1) {
  auto svc_controller = std::make_unique<ScalableVideoL2T1Ksvc>(GetParam());

  std::vector<EncodedImage> encoded_frames;
  TestAv1Encoder encoder(std::move(svc_controller));
  VideoFrameGenerator generator;
  // Encode 2 superframes into 4 layer frames.
  encoder.EncodeAndAppend(generator.NextFrame(), encoded_frames);
  encoder.EncodeAndAppend(generator.NextFrame(), encoded_frames);
  ASSERT_THAT(encoded_frames, SizeIs(4));

  // Decode upper spatial layers, i.e. frames 0, 1, 3.
  // S1 1--3
  //    |
  // S0 0--2
  TestAv1Decoder decoder;
  EXPECT_EQ(decoder.Decode(encoded_frames[0]), WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(decoder.Decode(encoded_frames[1]), WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(decoder.Decode(encoded_frames[3]), WEBRTC_VIDEO_CODEC_OK);
}

INSTANTIATE_TEST_SUITE_P(Svc, LibaomAv1KSvcTest, Values(3, 4));

}  // namespace
}  // namespace webrtc
