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
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "api/test/mock_video_decoder.h"
#include "api/test/mock_video_decoder_factory.h"
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
using CodedVideoSource = VideoCodecTester::CodedVideoSource;
using LayerSettings = EncodingSettings::LayerSettings;
using LayerId = EncodingSettings::LayerId;
using FramesSettings = VideoCodecTester::FramesSettings;
using DecoderSettings = VideoCodecTester::DecoderSettings;
using EncoderSettings = VideoCodecTester::EncoderSettings;
using PacingSettings = VideoCodecTester::PacingSettings;
using PacingMode = PacingSettings::PacingMode;

constexpr int kSourceWidth = 2;
constexpr int kSourceHeight = 2;
constexpr Frequency k90kHz = Frequency::Hertz(90000);

struct PacingTestParams {
  PacingSettings pacing_settings;
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
    for (int i = 0; i < 3 * kSourceWidth * kSourceHeight / 2; ++i) {
      fwrite("x", 1, 1, file);
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
      .WillByDefault([](const SdpVideoFormat&) {
        return std::make_unique<NiceMock<MockVideoEncoder>>();
      });

  FramesSettings frames_settings = VideoCodecTester::CreateFramesSettings(
      "VP8", "L1T1", kSourceWidth, kSourceHeight, /*layer_bitrates_kbps=*/{128},
      test_params_.framerate.hertz<double>(), test_params_.num_frames);

  EncoderSettings encoder_settings;
  encoder_settings.pacing_settings = test_params_.pacing_settings;

  VideoCodecTesterImpl tester;
  auto fs = tester
                .RunEncodeTest(video_source, &encoder_factory, encoder_settings,
                               frames_settings)
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

  NiceMock<MockVideoDecoderFactory> decoder_factory;
  ON_CALL(decoder_factory, CreateVideoDecoder(_))
      .WillByDefault([](const SdpVideoFormat&) {
        return std::make_unique<NiceMock<MockVideoDecoder>>();
      });

  FramesSettings frames_settings = VideoCodecTester::CreateFramesSettings(
      "VP8", "L1T1", kSourceWidth, kSourceHeight, /*layer_bitrates_kbps=*/{128},
      test_params_.framerate.hertz<double>(), test_params_.num_frames);

  DecoderSettings decoder_settings;
  decoder_settings.pacing_settings = test_params_.pacing_settings;

  VideoCodecTesterImpl tester;
  auto fs = tester
                .RunDecodeTest(&video_source, &decoder_factory,
                               decoder_settings, frames_settings)
                ->Slice();
  ASSERT_EQ(static_cast<int>(fs.size()), test_params_.num_frames);

  for (size_t i = 1; i < fs.size(); ++i) {
    int delta_ms = (fs[i].decode_start - fs[i - 1].decode_start).ms();
    EXPECT_NEAR(delta_ms, test_params_.expected_delta_ms[i - 1], 20);
  }
}

INSTANTIATE_TEST_SUITE_P(
    DISABLED_All,
    VideoCodecTesterImplPacingTest,
    ::testing::ValuesIn(
        {// No pacing.
         PacingTestParams({.pacing_settings = {.mode = PacingMode::kNoPacing},
                           .framerate = Frequency::Hertz(10),
                           .num_frames = 3,
                           .expected_delta_ms = {0, 0}}),
         // Real-time pacing.
         PacingTestParams({.pacing_settings = {.mode = PacingMode::kRealTime},
                           .framerate = Frequency::Hertz(10),
                           .num_frames = 3,
                           .expected_delta_ms = {100, 100}}),
         // Pace with specified constant rate.
         PacingTestParams(
             {.pacing_settings = {.mode = PacingMode::kConstantRate,
                                  .constant_rate = Frequency::Hertz(20)},
              .framerate = Frequency::Hertz(10),
              .num_frames = 3,
              .expected_delta_ms = {50, 50}})}));
}  // namespace test
}  // namespace webrtc
