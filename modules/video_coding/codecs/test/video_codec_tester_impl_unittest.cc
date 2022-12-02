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

#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/sleep.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::_;
using ::testing::Bool;
using ::testing::Return;

using TestDecoder = VideoCodecTester::TestDecoder;
using TestEncoder = VideoCodecTester::TestEncoder;
using TestCodedVideoSource = VideoCodecTester::TestCodedVideoSource;
using TestRawVideoSource = VideoCodecTester::TestRawVideoSource;
using DecodeSettings = VideoCodecTester::DecodeSettings;
using EncodeSettings = VideoCodecTester::EncodeSettings;
using PacingSettings = VideoCodecTester::PacingSettings;
using PacingMode = VideoCodecTester::PacingMode;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

VideoFrame CreateVideoFrame(uint32_t timestamp_rtp) {
  rtc::scoped_refptr<I420Buffer> buffer(I420Buffer::Create(2, 2));
  return VideoFrame::Builder()
      .set_video_frame_buffer(buffer)
      .set_timestamp_rtp(timestamp_rtp)
      .build();
}
/*
EncodedImage CreateEncodedImage(uint32_t timestamp_rtp) {
  EncodedImage encoded_image;
  encoded_image.SetTimestamp(timestamp_rtp);
  return encoded_image;
}*/

class MockTestRawVideoSource : public TestRawVideoSource {
 public:
  MockTestRawVideoSource(std::vector<int> timestamp_ms,
                         std::vector<int> pacing_time_ms)
      : timestamp_ms_(timestamp_ms),
        pacing_time_ms_(pacing_time_ms),
        num_frames_(timestamp_ms_.size()),
        frame_num_(0),
        timestamp_rtp_(0) {}

  absl::optional<VideoFrame> PullFrame() override {
    if (frame_num_ >= num_frames_) {
      return absl::nullopt;
    }

    int slee_ms = frame_num_ == 0 ? pacing_time_ms_[0]
                                  : pacing_time_ms_[frame_num_] -
                                        pacing_time_ms_[frame_num_ - 1];
    SleepMs(slee_ms);

    timestamp_rtp_ =
        k90kHz.hertz() * timestamp_ms_[frame_num_] / rtc::kNumMillisecsPerSec;

    ++frame_num_;
    return CreateVideoFrame(timestamp_rtp_);
  }
  MOCK_METHOD(VideoFrame, GetFrame, (uint32_t timestamp_rtp), (override));

 protected:
  std::vector<int> timestamp_ms_;
  std::vector<int> pacing_time_ms_;
  int num_frames_;
  int frame_num_;
  uint32_t timestamp_rtp_;
};

class MockTestCodedVideoSource : public TestCodedVideoSource {
 public:
  MOCK_METHOD(absl::optional<EncodedImage>, PullFrame, (), (override));
};

class MockTestDecoder : public TestDecoder {
 public:
  MOCK_METHOD(void,
              Decode,
              (const EncodedImage& frame, DecodeCallback callback),
              (override));
};

class MockTestEncoder : public TestEncoder {
 public:
  MOCK_METHOD(void,
              Encode,
              (const VideoFrame& frame, EncodeCallback callback),
              (override));
};
}  // namespace

class VideoCodecTesterImplPacingTest
    : public ::testing::TestWithParam<std::tuple<PacingSettings,
                                                 std::vector<int>,
                                                 std::vector<int>,
                                                 std::vector<int>>> {};

TEST_P(VideoCodecTesterImplPacingTest, PaceEncode) {
  PacingSettings pacing_settings = std::get<0>(GetParam());
  std::vector<int> frame_timestamp_ms = std::get<1>(GetParam());
  std::vector<int> pacing_time_ms = std::get<2>(GetParam());
  std::vector<int> expected_encode_start_ms = std::get<3>(GetParam());

  // TODO(ssilkin): Use EXPECT_CALL? Otherwise need to implement
  // MockTestCodedVideoSource as well.
  auto video_source = std::make_unique<MockTestRawVideoSource>(
      frame_timestamp_ms, pacing_time_ms);
  auto encoder = std::make_unique<MockTestEncoder>();

  EncodeSettings encode_settings;
  encode_settings.pacing = pacing_settings;

  VideoCodecTesterImpl tester;
  auto fs = tester
                .RunEncodeTest(std::move(video_source), std::move(encoder),
                               encode_settings)
                ->GetFrameStatistics();
  ASSERT_EQ(fs.size(), frame_timestamp_ms.size());

  for (size_t i = 0; i < fs.size(); ++i) {
    int encode_start_ms = (fs[i].encode_start_ns - fs[0].encode_start_ns) /
                          rtc::kNumNanosecsPerMillisec;
    EXPECT_NEAR(encode_start_ms, expected_encode_start_ms[i], 10);
  }
}

#if 0
TEST_P(VideoCodecTesterImplPacingTest, PaceDecode) {
  constexpr TimeDelta frame_duration = TimeDelta::Millis(100);
  PacingSettings pacing_settings = std::get<0>(GetParam());
  TimeDelta expected_pacing_time = std::get<1>(GetParam());

  auto video_source = std::make_unique<MockTestCodedVideoSource>();

  EXPECT_CALL(*video_source, PullFrame)
      .WillOnce(Return(CreateEncodedImage(/*timestamp_rtp=*/0)))
      .WillOnce(Return(CreateEncodedImage(
          /*timestamp_rtp=*/frame_duration.ms() * k90kHz.hertz() / 1000)))
      .WillOnce(Return(absl::nullopt));

  auto decoder = std::make_unique<MockTestDecoder>();
  DecodeSettings decode_settings;
  decode_settings.pacing = pacing_settings;
  VideoCodecTesterImpl tester;
  auto fs = tester
                .RunDecodeTest(std::move(video_source), std::move(decoder),
                               decode_settings)
                ->GetFrameStatistics();
  ASSERT_EQ(2u, fs.size());

  TimeDelta actual_pacing_time =
      TimeDelta::Micros((fs[1].decode_start_ns - fs[0].decode_start_ns) / 1000);
  EXPECT_NEAR(actual_pacing_time.ms(), expected_pacing_time.ms(), 10);
}
#endif

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoCodecTesterImplPacingTest,
    ::testing::ValuesIn(
        {std::make_tuple(PacingSettings({.mode = PacingMode::kNoPacing,
                                         .rate = Frequency::Zero()}),
                         /*frame_timestamp_ms=*/std::vector<int>{0, 100},
                         /*pacing_time_ms=*/std::vector<int>{0, 0},
                         /*expected_encode_start_ms=*/std::vector<int>{0, 0}),
         std::make_tuple(PacingSettings({.mode = PacingMode::kRealTime,
                                         .rate = Frequency::Zero()}),
                         /*frame_timestamp_ms=*/std::vector<int>{0, 100},
                         /*pacing_time_ms=*/std::vector<int>{0, 0},
                         /*expected_encode_start_ms=*/std::vector<int>{0, 100}),
         std::make_tuple(PacingSettings({.mode = PacingMode::kRealTime,
                                         .rate = Frequency::Zero()}),
                         /*frame_timestamp_ms=*/std::vector<int>{0, 100},
                         /*pacing_time_ms=*/std::vector<int>{0, 200},
                         /*expected_encode_start_ms=*/std::vector<int>{0, 200}),
         std::make_tuple(PacingSettings({.mode = PacingMode::kConstRate,
                                         .rate = Frequency::Hertz(20)}),
                         /*frame_timestamp_ms=*/std::vector<int>{0, 100},
                         /*pacing_time_ms=*/std::vector<int>{0, 0},
                         /*expected_encode_start_ms=*/std::vector<int>{0, 50}),
         std::make_tuple(
             PacingSettings({.mode = PacingMode::kConstRate,
                             .rate = Frequency::Hertz(20)}),
             /*frame_timestamp_ms=*/std::vector<int>{0, 100},
             /*pacing_time_ms=*/std::vector<int>{0, 200},
             /*expected_encode_start_ms=*/std::vector<int>{0, 200})}));
}  // namespace test
}  // namespace webrtc
