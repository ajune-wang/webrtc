/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/video_codec_tester.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "api/test/mock_video_decoder.h"
#include "api/test/mock_video_decoder_factory.h"
#include "api/test/mock_video_encoder.h"
#include "api/test/mock_video_encoder_factory.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "media/engine/fake_video_codec_factory.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/time_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_writer.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SizeIs;

using VideoCodecStats = VideoCodecTester::VideoCodecStats;
using VideoSourceSettings = VideoCodecTester::VideoSourceSettings;
using CodedVideoSource = VideoCodecTester::CodedVideoSource;
using EncodingSettings = VideoCodecTester::EncodingSettings;
using LayerSettings = EncodingSettings::LayerSettings;
using LayerId = VideoCodecTester::LayerId;
using DecoderSettings = VideoCodecTester::DecoderSettings;
using EncoderSettings = VideoCodecTester::EncoderSettings;
using PacingSettings = VideoCodecTester::PacingSettings;
using PacingMode = PacingSettings::PacingMode;
using Filter = VideoCodecStats::Filter;
using Frame = VideoCodecTester::VideoCodecStats::Frame;
using Stream = VideoCodecTester::VideoCodecStats::Stream;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

EncodedImage CreateEncodedImage(uint32_t timestamp_rtp) {
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(timestamp_rtp);
  return encoded_image;
}

class MockCodedVideoSource : public CodedVideoSource {
 public:
  MockCodedVideoSource(int num_frames, Frequency framerate)
      : num_frames_(num_frames), frame_num_(0), framerate_(framerate) {}

  absl::optional<EncodedImage> PullFrame() override {
    if (frame_num_ >= num_frames_) {
      return absl::nullopt;
    }
    uint32_t timestamp_rtp = frame_num_ * k90kHz / framerate_;
    ++frame_num_;
    return CreateEncodedImage(timestamp_rtp);
  }

 private:
  int num_frames_;
  int frame_num_;
  Frequency framerate_;
};

}  // namespace

class VideoCodecTesterTest : public ::testing::Test {
 public:
  const int kWidth = 2;
  const int kHeight = 2;
  const int kNumFrames = 3;
  const DataRate kBitrate = DataRate::BytesPerSec(300);
  const Frequency kFramerate = Frequency::Hertz(30);

 protected:
  std::string CreateYuvFile(int width, int height, int num_frames) {
    std::string path = webrtc::test::TempFilename(
        webrtc::test::OutputPath(), "video_codec_tester_unittest");
    FILE* file = fopen(path.c_str(), "wb");
    for (int frame_num = 0; frame_num < num_frames; ++frame_num) {
      for (int i = 0; i < 3 * width * height / 2; ++i) {
        fwrite("x", 1, 1, file);
      }
    }
    fclose(file);
    return path;
  }

  std::unique_ptr<VideoCodecStats> RunEncodeTest(
      std::vector<std::vector<Frame>> frames,
      ScalabilityMode scalability_mode) {
    int num_frames = static_cast<int>(frames.size());
    std::string source_yuv_path = CreateYuvFile(kWidth, kHeight, num_frames);
    VideoSourceSettings source_settings{
        .file_path = source_yuv_path,
        .resolution = {.width = kWidth, .height = kHeight},
        .framerate = kFramerate};

    int num_encoded_frames = 0;
    EncodedImageCallback* encoded_frame_callback;
    NiceMock<MockVideoEncoderFactory> encoder_factory;
    ON_CALL(encoder_factory, CreateVideoEncoder)
        .WillByDefault([&](const SdpVideoFormat&) {
          auto encoder = std::make_unique<NiceMock<MockVideoEncoder>>();
          ON_CALL(*encoder, RegisterEncodeCompleteCallback)
              .WillByDefault([&](EncodedImageCallback* callback) {
                encoded_frame_callback = callback;
                return WEBRTC_VIDEO_CODEC_OK;
              });
          ON_CALL(*encoder, Encode)
              .WillByDefault([&](const VideoFrame& input_frame,
                                 const std::vector<VideoFrameType>*) {
                for (const Frame& frame : frames[num_encoded_frames]) {
                  EncodedImage encoded_frame;
                  encoded_frame._encodedWidth = frame.width;
                  encoded_frame._encodedHeight = frame.height;
                  encoded_frame.SetRtpTimestamp(input_frame.timestamp());
                  encoded_frame.SetSpatialIndex(frame.layer_id.spatial_idx);
                  encoded_frame.SetEncodedData(
                      EncodedImageBuffer::Create(frame.frame_size.bytes()));
                  encoded_frame_callback->OnEncodedImage(
                      encoded_frame,
                      /*codec_specific_info=*/nullptr);
                }
                ++num_encoded_frames;
                return WEBRTC_VIDEO_CODEC_OK;
              });
          return encoder;
        });

    // Generate encoding settings from provided frames.
    std::map<uint32_t, EncodingSettings> encoding_settings;
    for (int frame_num = 0; frame_num < num_frames; ++frame_num) {
      std::map<LayerId, LayerSettings> layers_settings;
      for (const auto& frame : frames[frame_num]) {
        layers_settings.emplace(
            frame.layer_id,
            LayerSettings{
                .resolution = {.width = frame.width, .height = frame.height},
                .framerate = kFramerate,
                .bitrate = kBitrate});
      }
      encoding_settings.emplace(
          frames[frame_num][0].timestamp_rtp,
          EncodingSettings{.scalability_mode = scalability_mode,
                           .layers_settings = layers_settings});
    }

    EncoderSettings encoder_settings;
    std::unique_ptr<VideoCodecStats> stats = VideoCodecTester::RunEncodeTest(
        source_settings, &encoder_factory, encoder_settings, encoding_settings);
    remove(source_yuv_path.c_str());
    return stats;
  }
};

TEST_F(VideoCodecTesterTest, Slice) {
  std::unique_ptr<VideoCodecStats> stats =
      RunEncodeTest({{{.timestamp_rtp = 0,
                       .layer_id = {.spatial_idx = 0, .temporal_idx = 0},
                       .width = 320,
                       .height = 180,
                       .frame_size = DataSize::Bytes(1)}},
                     {{.timestamp_rtp = 1,
                       .layer_id = {.spatial_idx = 0, .temporal_idx = 0},
                       .width = 320,
                       .height = 180,
                       .frame_size = DataSize::Bytes(2)}}},
                    ScalabilityMode::kL1T1);
  std::vector<Frame> slice = stats->Slice({}, /*merge=*/false);
  ASSERT_THAT(slice, SizeIs(2));
  EXPECT_EQ(slice[0].timestamp_rtp, 0u);
  EXPECT_EQ(slice[1].timestamp_rtp, 1u);

  slice = stats->Slice({.min_timestamp_rtp = 1}, /*merge=*/false);
  ASSERT_THAT(slice, SizeIs(1));
  EXPECT_EQ(slice[0].timestamp_rtp, 1u);

  slice = stats->Slice({.max_timestamp_rtp = 0}, /*merge=*/false);
  ASSERT_THAT(slice, SizeIs(1));
  EXPECT_EQ(slice[0].timestamp_rtp, 0u);
}

class VideoCodecTesterTestPacing
    : public ::testing::TestWithParam<std::tuple<PacingSettings, int>> {
 public:
  const int kSourceWidth = 2;
  const int kSourceHeight = 2;
  const int kNumFrames = 3;
  const int kBitrateKbps = 128;
  const Frequency kFramerate = Frequency::Hertz(10);

  void SetUp() override {
    source_yuv_file_path_ = webrtc::test::TempFilename(
        webrtc::test::OutputPath(), "video_codec_tester_impl_unittest");
    FILE* file = fopen(source_yuv_file_path_.c_str(), "wb");
    for (int i = 0; i < 3 * kSourceWidth * kSourceHeight / 2; ++i) {
      fwrite("x", 1, 1, file);
    }
    fclose(file);
  }

 protected:
  std::string source_yuv_file_path_;
};

TEST_P(VideoCodecTesterTestPacing, PaceEncode) {
  auto [pacing_settings, expected_delta_ms] = GetParam();
  VideoSourceSettings video_source{
      .file_path = source_yuv_file_path_,
      .resolution = {.width = kSourceWidth, .height = kSourceHeight},
      .framerate = kFramerate};

  NiceMock<MockVideoEncoderFactory> encoder_factory;
  ON_CALL(encoder_factory, CreateVideoEncoder(_))
      .WillByDefault([](const SdpVideoFormat&) {
        return std::make_unique<NiceMock<MockVideoEncoder>>();
      });

  std::map<uint32_t, EncodingSettings> encoding_settings =
      VideoCodecTester::CreateEncodingSettings("VP8", "L1T1", kSourceWidth,
                                               kSourceHeight, {kBitrateKbps},
                                               kFramerate.hertz(), kNumFrames);

  EncoderSettings encoder_settings;
  encoder_settings.pacing_settings = pacing_settings;
  std::vector<Frame> frames =
      VideoCodecTester::RunEncodeTest(video_source, &encoder_factory,
                                      encoder_settings, encoding_settings)
          ->Slice(/*filter=*/{}, /*merge=*/false);
  ASSERT_THAT(frames, SizeIs(kNumFrames));
  EXPECT_NEAR((frames[1].encode_start - frames[0].encode_start).ms(),
              expected_delta_ms, 10);
  EXPECT_NEAR((frames[2].encode_start - frames[1].encode_start).ms(),
              expected_delta_ms, 10);
}

TEST_P(VideoCodecTesterTestPacing, PaceDecode) {
  auto [pacing_settings, expected_delta_ms] = GetParam();
  MockCodedVideoSource video_source(kNumFrames, kFramerate);

  NiceMock<MockVideoDecoderFactory> decoder_factory;
  ON_CALL(decoder_factory, CreateVideoDecoder(_))
      .WillByDefault([](const SdpVideoFormat&) {
        return std::make_unique<NiceMock<MockVideoDecoder>>();
      });

  DecoderSettings decoder_settings;
  decoder_settings.pacing_settings = pacing_settings;
  std::vector<Frame> frames =
      VideoCodecTester::RunDecodeTest(&video_source, &decoder_factory,
                                      decoder_settings, SdpVideoFormat("VP8"))
          ->Slice(/*filter=*/{}, /*merge=*/false);
  ASSERT_THAT(frames, SizeIs(kNumFrames));
  EXPECT_NEAR((frames[1].decode_start - frames[0].decode_start).ms(),
              expected_delta_ms, 10);
  EXPECT_NEAR((frames[2].decode_start - frames[1].decode_start).ms(),
              expected_delta_ms, 10);
}

INSTANTIATE_TEST_SUITE_P(
    DISABLED_All,
    VideoCodecTesterTestPacing,
    ::testing::Values(
        // No pacing.
        std::make_tuple(PacingSettings{.mode = PacingMode::kNoPacing},
                        /*expected_delta_ms=*/0),
        // Real-time pacing.
        std::make_tuple(PacingSettings{.mode = PacingMode::kRealTime},
                        /*expected_delta_ms=*/100),
        // Pace with specified constant rate.
        std::make_tuple(PacingSettings{.mode = PacingMode::kConstantRate,
                                       .constant_rate = Frequency::Hertz(20)},
                        /*expected_delta_ms=*/50)));
}  // namespace test
}  // namespace webrtc
