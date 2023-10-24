/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_tester_impl.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "api/test/mock_video_decoder.h"
#include "api/test/mock_video_encoder.h"
#include "api/test/mock_video_encoder_factory.h"
#include "api/test/video_codec_stats.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "media/engine/fake_video_codec_factory.h"
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

using VideoSourceSettings = VideoCodecTester::VideoSourceSettings;
using LayerId = VideoCodecTester::EncodingSettings::LayerId;
using LayerSettings = VideoCodecTester::EncodingSettings::LayerSettings;
using EncodingSettings = VideoCodecTester::EncodingSettings;
using FrameSettings = VideoCodecTester::FrameSettings;
using CodedVideoSource = VideoCodecTester::CodedVideoSource;
using DecoderSettings = VideoCodecTester::DecoderSettings;
using EncoderSettings = VideoCodecTester::EncoderSettings;

constexpr int kSourceWidth = 2;
constexpr int kSourceHeight = 2;
constexpr Frequency k90kHz = Frequency::Hertz(90000);

struct PacingTestParams {
  bool hardware_codec;
  Frequency framerate;
  int num_frames;
  std::vector<int> expected_delta_ms;
};

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

class VideoCodecTesterImplPacingTest
    : public ::testing::TestWithParam<PacingTestParams> {
 public:
  VideoCodecTesterImplPacingTest() : test_params_(GetParam()) {}

  void SetUp() override {
    source_yuv_file_path_ = webrtc::test::TempFilename(
        webrtc::test::OutputPath(), "video_codec_tester_impl_unittest");
    FILE* file = fopen(source_yuv_file_path_.c_str(), "wb");
    for (int i = 0; i < kSourceWidth * kSourceHeight; ++i) {
      fwrite("Y", 1, 1, file);
    }
    int chroma_pixel_count = (kSourceWidth + 1 / 2) * (kSourceHeight + 1) / 2;
    for (int i = 0; i < chroma_pixel_count; ++i) {
      fwrite("U", 1, 1, file);
    }
    for (int i = 0; i < chroma_pixel_count; ++i) {
      fwrite("V", 1, 1, file);
    }
    fclose(file);
  }

 protected:
  PacingTestParams test_params_;
  std::string source_yuv_file_path_;
};

TEST_P(VideoCodecTesterImplPacingTest, PaceEncode) {
  VideoSourceSettings video_source{
      .file_path = source_yuv_file_path_,
      .resolution = {.width = kSourceWidth, .height = kSourceHeight},
      .framerate = test_params_.framerate};

  NiceMock<MockVideoEncoderFactory> encoder_factory;
  ON_CALL(encoder_factory, CreateVideoEncoder(_))
      .WillByDefault([this](const SdpVideoFormat&) {
        auto encoder = std::make_unique<NiceMock<MockVideoEncoder>>();
        ON_CALL(*encoder, GetEncoderInfo).WillByDefault([this]() {
          VideoEncoder::EncoderInfo info;
          info.is_hardware_accelerated = test_params_.hardware_codec;
          return info;
        });
        return encoder;
      });

  FrameSettings frame_settings;
  uint32_t timestamp_rtp = 0;
  for (int frame_num = 0; frame_num < test_params_.num_frames; ++frame_num) {
    std::map<LayerId, LayerSettings> layer_settings;
    layer_settings.emplace(
        LayerId{.spatial_idx = 0, .temporal_idx = 0},
        LayerSettings{
            .resolution = {.width = kSourceWidth, .height = kSourceHeight},
            .framerate = test_params_.framerate,
            .bitrate = DataRate::KilobitsPerSec(128)});

    frame_settings.emplace(
        timestamp_rtp,
        EncodingSettings{.sdp_video_format = SdpVideoFormat("VP8"),
                         .scalability_mode = ScalabilityMode::kL1T1,
                         .layer_settings = layer_settings});
    timestamp_rtp += k90kHz / test_params_.framerate;
  }

  EncoderSettings encoder_settings;
  VideoCodecTesterImpl tester;
  auto fs = tester
                .RunEncodeTest(video_source, &encoder_factory, encoder_settings,
                               frame_settings)
                ->Slice();
  ASSERT_EQ(static_cast<int>(fs.size()), test_params_.num_frames);

  for (size_t i = 1; i < fs.size(); ++i) {
    int delta_ms = (fs[i].encode_start - fs[i - 1].encode_start).ms();
    EXPECT_NEAR(delta_ms, test_params_.expected_delta_ms[i - 1], 10);
  }
}

TEST_P(VideoCodecTesterImplPacingTest, PaceDecode) {
  MockCodedVideoSource video_source(test_params_.num_frames,
                                    test_params_.framerate);

  NiceMock<MockVideoDecoder> decoder;
  ON_CALL(decoder, GetDecoderInfo).WillByDefault([this]() {
    VideoDecoder::DecoderInfo info;
    info.is_hardware_accelerated = test_params_.hardware_codec;
    return info;
  });

  DecoderSettings decoder_settings;
  VideoCodecTesterImpl tester;
  auto fs =
      tester.RunDecodeTest(&video_source, &decoder, decoder_settings)->Slice();
  ASSERT_EQ(static_cast<int>(fs.size()), test_params_.num_frames);

  // TODO: use Aggregate().GetTimedSamples(). Then we don't need to expose Slice
  // in stats API.
  for (size_t i = 1; i < fs.size(); ++i) {
    int delta_ms = (fs[i].decode_start - fs[i - 1].decode_start).ms();
    EXPECT_NEAR(delta_ms, test_params_.expected_delta_ms[i - 1], 20);
  }
}

INSTANTIATE_TEST_SUITE_P(
    DISABLED_All,
    VideoCodecTesterImplPacingTest,
    ::testing::ValuesIn({
        // No pacing.
        PacingTestParams({.hardware_codec = false,
                          .framerate = Frequency::Hertz(10),
                          .num_frames = 3,
                          .expected_delta_ms = {0, 0}}),
        // Real-time pacing.
        PacingTestParams({.hardware_codec = true,
                          .framerate = Frequency::Hertz(10),
                          .num_frames = 3,
                          .expected_delta_ms = {100, 100}}),
    }));
}  // namespace test
}  // namespace webrtc
