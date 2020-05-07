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
#include "api/test/mock_video_decoder.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/av1/frame_dependencies_controller_full_svc.h"
#include "modules/video_coding/codecs/av1/frame_dependencies_controller_single_stream.h"
#include "modules/video_coding/codecs/av1/libaom_av1_decoder.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::Combine;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::SizeIs;
using ::testing::Values;
using ::testing::ValuesIn;
using ::testing::WithArgs;

// Use small resolution to make tests faster.
constexpr int kWidth = 320;
constexpr int kHeight = 180;
constexpr int kFramerate = 30;

std::vector<VideoFrame> CreateFrames(size_t num_frames) {
  std::vector<VideoFrame> frames;
  frames.reserve(num_frames);

  auto input_frame_generator = test::CreateSquareFrameGenerator(
      kWidth, kHeight, test::FrameGeneratorInterface::OutputType::kI420,
      absl::nullopt);
  uint32_t timestamp = 4321;
  for (size_t i = 0; i < num_frames; ++i) {
    frames.push_back(
        VideoFrame::Builder()
            .set_video_frame_buffer(input_frame_generator->NextFrame().buffer)
            .set_timestamp_rtp(timestamp += 90000 / kFramerate)
            .build());
  }
  return frames;
}

class TestDecoder {
 public:
  TestDecoder() = default;
  TestDecoder(const TestDecoder&) = delete;
  TestDecoder& operator=(const TestDecoder&) = delete;

  bool Init(int decoder_id) {
    decoder_id_ = decoder_id;
    decoder_ = CreateLibaomAv1Decoder();
    if (decoder_ == nullptr) {
      ADD_FAILURE() << "Failed to create a decoder " << decoder_id_;
      return false;
    }
    // It is unclear in advance what expectation should be set for number of
    // decoded frames.
    // Use wildcard matchest to distinguish Decoded overload.
    ON_CALL(on_decoded_, Decoded(_, _, _)).WillByDefault([this] {
      ++num_decoded_callbacks_;
    });

    EXPECT_EQ(
        decoder_->InitDecode(/*codec_settings=*/nullptr, /*number_of_cores=*/1),
        WEBRTC_VIDEO_CODEC_OK);
    EXPECT_EQ(decoder_->RegisterDecodeCompleteCallback(&on_decoded_),
              WEBRTC_VIDEO_CODEC_OK);
    return true;
  }

  void Decode(int64_t frame_id, const EncodedImage& image) {
    passed_frames_.push_back(frame_id);
    auto error = decoder_->Decode(image, false, image.capture_time_ms_ / 1000);
    if (error == WEBRTC_VIDEO_CODEC_OK) {
      decoded_frames_.push_back(frame_id);
    } else {
      ADD_FAILURE() << "Failed to decode frame id " << frame_id
                    << " by decoder N" << decoder_id_ << " with error code "
                    << error;
    }
  }

  const std::vector<int64_t>& passed_frames() const { return passed_frames_; }
  const std::vector<int64_t>& decoded_frames() const { return decoded_frames_; }
  size_t num_decoded_callbacks() const { return num_decoded_callbacks_; }

 private:
  size_t decoder_id_ = 0;
  std::vector<int64_t> passed_frames_;
  std::vector<int64_t> decoded_frames_;
  size_t num_decoded_callbacks_ = 0;
  NiceMock<MockDecodedImageCallback> on_decoded_;
  std::unique_ptr<VideoDecoder> decoder_;
};

class TestAv1Encoder {
 public:
  struct Encoded {
    EncodedImage encoded_image;
    CodecSpecificInfo codec_specific_info;
  };

  explicit TestAv1Encoder(FrameDependenciesController* controller)
      : encoder_(CreateLibaomAv1Encoder(controller)) {
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
  // This class requires pointer stability and thus not copyable nor movable.
  TestAv1Encoder(const TestAv1Encoder&) = delete;
  TestAv1Encoder& operator=(const TestAv1Encoder&) = delete;

  void EncodeAndAppend(const VideoFrame& frame, std::vector<Encoded>* encoded) {
    callback_.SetEncodeStorage(encoded);
    std::vector<VideoFrameType> frame_types = {
        VideoFrameType::kVideoFrameDelta};
    EXPECT_EQ(encoder_->Encode(frame, &frame_types), WEBRTC_VIDEO_CODEC_OK);
    // Prefer to crash checking nullptr rather than writing to random memory.
    callback_.SetEncodeStorage(nullptr);
  }

 private:
  class EncoderCallback : public EncodedImageCallback {
   public:
    void SetEncodeStorage(std::vector<Encoded>* storage) { storage_ = storage; }

   private:
    Result OnEncodedImage(
        const EncodedImage& encoded_image,
        const CodecSpecificInfo* codec_specific_info,
        const RTPFragmentationHeader* /*fragmentation*/) override {
      RTC_CHECK(storage_);
      storage_->push_back({encoded_image, *codec_specific_info});
      return Result(Result::Error::OK);
    }

    std::vector<Encoded>* storage_ = nullptr;
  };

  EncoderCallback callback_;
  std::unique_ptr<VideoEncoder> encoder_;
};

TEST(LibaomAv1Test, EncodeDecode) {
  // Assemble
  TestDecoder client;
  ASSERT_TRUE(client.Init(0));
  TestAv1Encoder encoder(nullptr);

  // Act
  std::vector<TestAv1Encoder::Encoded> encoded_frames;
  for (const VideoFrame& frame : CreateFrames(/*num_frames=*/12)) {
    encoder.EncodeAndAppend(frame, &encoded_frames);
  }
  for (int64_t frame_idx = 0;
       frame_idx < static_cast<int64_t>(encoded_frames.size()); ++frame_idx) {
    client.Decode(frame_idx, encoded_frames[frame_idx].encoded_image);
  }

  // Assert
  ASSERT_THAT(client.passed_frames(), Not(IsEmpty()));
  EXPECT_THAT(client.decoded_frames(),
              ElementsAreArray(client.passed_frames()));
  EXPECT_EQ(client.num_decoded_callbacks(), client.decoded_frames().size());
}

using StreamControllerFactory =
    std::function<std::unique_ptr<FrameDependenciesController>()>;

class LibaomAv1SvcTest
    : public ::testing::TestWithParam<StreamControllerFactory> {
 public:
  LibaomAv1SvcTest() : controller_(GetParam()()) {}

  FrameDependenciesController* Controller() { return controller_.get(); }

 private:
  const std::unique_ptr<FrameDependenciesController> controller_;
};

TEST_P(LibaomAv1SvcTest, EncodeAndDecodeAllDecodeTarget) {
  // Assemble
  size_t num_decode_targets =
      Controller()->DependencyStructure().num_decode_targets;
  // Though it is nicer to have one test per decoder, it makes tests slow
  // because frames need to be reencoded again and again. So instead this test
  // generates and encoders frames once, and then pass it to multiple
  // independent decoders.
  std::vector<TestDecoder> clients(num_decode_targets);
  for (size_t i = 0; i < num_decode_targets; ++i) {
    ASSERT_TRUE(clients[i].Init(i));
  }

  TestAv1Encoder encoder(Controller());

  // Act
  std::vector<TestAv1Encoder::Encoded> encoded_frames;
  for (const VideoFrame& frame : CreateFrames(/*num_frames=*/12)) {
    encoder.EncodeAndAppend(frame, &encoded_frames);
  }
  for (int64_t frame_idx = 0;
       frame_idx < static_cast<int64_t>(encoded_frames.size()); ++frame_idx) {
    const auto& encoded_image = encoded_frames[frame_idx].encoded_image;
    const auto& codec_specific_info =
        encoded_frames[frame_idx].codec_specific_info;
    if (!codec_specific_info.generic_frame_info) {
      ADD_FAILURE() << "No generic frame info is provided for frame#"
                    << frame_idx;
      continue;
    }

    const auto& dtis =
        codec_specific_info.generic_frame_info->decode_target_indications;
    if (dtis.size() != clients.size()) {
      ADD_FAILURE() << "Unexpected number of dtis " << dtis.size()
                    << " for frame#" << frame_idx << ". Expected "
                    << clients.size();
      continue;
    }

    for (size_t i = 0; i < clients.size(); ++i) {
      if (dtis[i] != webrtc::DecodeTargetIndication::kNotPresent) {
        clients[i].Decode(frame_idx, encoded_image);
      }
    }
  }

  // Assert
  for (const TestDecoder& client : clients) {
    ASSERT_THAT(client.passed_frames(), Not(IsEmpty()));
    EXPECT_THAT(client.decoded_frames(),
                ElementsAreArray(client.passed_frames()));
    EXPECT_EQ(client.num_decoded_callbacks(), client.decoded_frames().size());
  }
}

INSTANTIATE_TEST_SUITE_P(
    Svc,
    LibaomAv1SvcTest,
    Values(std::make_unique<FrameDependenciesControllerSingleStream>,
           std::make_unique<FrameDependenciesControllerFullSvc>));

}  // namespace
}  // namespace webrtc
