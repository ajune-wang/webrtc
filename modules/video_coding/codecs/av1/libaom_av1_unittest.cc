/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/test/create_frame_generator.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/mock_video_encoder.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/av1/libaom_av1_decoder.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::WithArgs;

// Use smaller resolution for this test to make them faster.
constexpr int kWidth = 320;
constexpr int kHeight = 180;
constexpr int kFramerate = 30;
constexpr int kRtpTicksPerSecond = 90000;

std::vector<VideoFrame> CreateFrames(size_t num_frames) {
  std::vector<VideoFrame> frames;
  frames.reserve(num_frames);

  auto input_frame_generator = test::CreateSquareFrameGenerator(
      kWidth, kHeight, test::FrameGeneratorInterface::OutputType::kI420,
      absl::nullopt);
  uint32_t timestamp = 1000;
  for (size_t i = 0; i < num_frames; ++i) {
    frames.push_back(
        VideoFrame::Builder()
            .set_video_frame_buffer(input_frame_generator->NextFrame().buffer)
            .set_timestamp_rtp(timestamp += kRtpTicksPerSecond / kFramerate)
            .build());
  }
  return frames;
}

class TestAv1Decoder {
 public:
  TestAv1Decoder() = default;
  // This class require pointer stability and thus not copyable nor movable.
  TestAv1Decoder(const TestAv1Decoder&) = delete;
  TestAv1Decoder& operator=(const TestAv1Decoder&) = delete;

  bool Init() {
    decoder_ = CreateLibaomAv1Decoder();
    if (decoder_ == nullptr) {
      ADD_FAILURE() << "Failed to create a decoder";
      return false;
    }
    EXPECT_EQ(decoder_->InitDecode(/*codec_settings=*/nullptr,
                                   /*number_of_cores=*/1),
              WEBRTC_VIDEO_CODEC_OK);
    EXPECT_EQ(decoder_->RegisterDecodeCompleteCallback(&callback_),
              WEBRTC_VIDEO_CODEC_OK);
    return true;
  }

  void Decode(int64_t frame_id, const EncodedImage& image) {
    passed_frames_.push_back(frame_id);
    int32_t error = decoder_->Decode(image, /*missing_frames=*/false,
                                     /*render_time_ms=*/image.capture_time_ms_);
    if (error != WEBRTC_VIDEO_CODEC_OK) {
      ADD_FAILURE() << "Failed to decode frame id " << frame_id
                    << " with error code " << error;
      return;
    }
    decoded_frames_.push_back(frame_id);
  }

  const std::vector<int64_t>& passed_frames() const { return passed_frames_; }
  const std::vector<int64_t>& decoded_frames() const { return decoded_frames_; }
  size_t num_decoder_callbacks() const { return callback_.num_called(); }

 private:
  // Decoder callback that only counter how many times it was called.
  class DecoderCallback : public DecodedImageCallback {
   public:
    size_t num_called() const { return num_called_; }

   private:
    int32_t Decoded(VideoFrame& /*decoded_image*/) override {
      ++num_called_;
      return 0;
    }
    void Decoded(VideoFrame& /*decoded_image*/,
                 absl::optional<int32_t> /*decode_time_ms*/,
                 absl::optional<uint8_t> /*qp*/) override {
      ++num_called_;
    }

    int num_called_ = 0;
  };

  std::vector<int64_t> passed_frames_;
  std::vector<int64_t> decoded_frames_;
  DecoderCallback callback_;
  std::unique_ptr<VideoDecoder> decoder_;
};

std::unique_ptr<VideoEncoder> CreateEncoder(EncodedImageCallback* callback) {
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  if (encoder == nullptr) {
    return encoder;
  }
  VideoCodec codec_settings;
  codec_settings.width = kWidth;
  codec_settings.height = kHeight;
  codec_settings.maxFramerate = kFramerate;
  VideoEncoder::Settings encoder_settings(
      VideoEncoder::Capabilities(/*loss_notification=*/false),
      /*number_of_cores=*/1, /*max_payload_size=*/1200);
  EXPECT_EQ(encoder->InitEncode(&codec_settings, encoder_settings),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(encoder->RegisterEncodeCompleteCallback(callback),
            WEBRTC_VIDEO_CODEC_OK);
  return encoder;
}

TEST(LibaomAv1Test, EncodeDecode) {
  // Assemble.
  TestAv1Decoder decoder;
  ASSERT_TRUE(decoder.Init());

  NiceMock<MockEncodedImageCallback> stub_encoded_callback;
  int64_t frame_number = 0;
  ON_CALL(stub_encoded_callback, OnEncodedImage)
      .WillByDefault(WithArgs<0>([&](const EncodedImage& image) {
        decoder.Decode(++frame_number, image);
        return EncodedImageCallback::Result(EncodedImageCallback::Result::OK);
      }));
  std::unique_ptr<VideoEncoder> encoder = CreateEncoder(&stub_encoded_callback);
  ASSERT_THAT(encoder, NotNull());

  // Act.
  std::vector<VideoFrameType> frame_types = {VideoFrameType::kVideoFrameDelta};
  for (const VideoFrame& frame : CreateFrames(/*num_frames=*/4)) {
    encoder->Encode(frame, &frame_types);
  }

  // Assert.
  EXPECT_THAT(decoder.passed_frames(), Not(IsEmpty()));
  EXPECT_THAT(decoder.decoded_frames(),
              ElementsAreArray(decoder.passed_frames()));
  EXPECT_EQ(decoder.num_decoder_callbacks(), decoder.decoded_frames().size());
}

}  // namespace
}  // namespace webrtc
