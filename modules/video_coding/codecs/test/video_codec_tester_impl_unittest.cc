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
#include <utility>

#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/time_utils.h"
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
using TestSettings = VideoCodecTester::TestSettings;

static const int kWaitTimeout = 5000;

class MockTestRawVideoSource : public TestRawVideoSource {
 public:
  MOCK_METHOD(absl::optional<VideoFrame>, PullFrame, (), (override));
  MOCK_METHOD(VideoFrame, GetFrame, (uint32_t timestamp_rtp), (override));
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

VideoFrame CreateVideoFrame(uint32_t timestamp_rtp) {
  rtc::scoped_refptr<I420Buffer> buffer(I420Buffer::Create(2, 2));
  return VideoFrame::Builder()
      .set_video_frame_buffer(buffer)
      .set_timestamp_rtp(timestamp_rtp)
      .build();
}

EncodedImage CreateEncodedImage(uint32_t timestamp_rtp) {
  EncodedImage encoded_image;
  encoded_image.SetTimestamp(timestamp_rtp);
  return encoded_image;
}
}  // namespace

class VideoCodecTesterImplTest : public ::testing::Test {
 public:
  void RunDecodeTest(std::unique_ptr<TestCodedVideoSource> video_source,
                     const TestSettings& test_settings,
                     std::unique_ptr<TestDecoder> decoder) {
    task_queue_.PostTask([this, video_source = std::move(video_source),
                          &test_settings,
                          decoder = std::move(decoder)]() mutable {
      VideoCodecTesterImpl tester;
      this->stats_ = tester.RunDecodeTest(std::move(video_source),
                                          test_settings, std::move(decoder));
    });
  }

  void RunEncodeTest(std::unique_ptr<TestRawVideoSource> video_source,
                     const TestSettings& test_settings,
                     std::unique_ptr<TestEncoder> encoder) {
    task_queue_.PostTask([this, video_source = std::move(video_source),
                          &test_settings,
                          encoder = std::move(encoder)]() mutable {
      VideoCodecTesterImpl tester;
      this->stats_ = tester.RunEncodeTest(std::move(video_source),
                                          test_settings, std::move(encoder));
    });
  }

  const VideoCodecTestStats* GetStats() {
    task_queue_.WaitForPreviouslyPostedTasks();
    return stats_.get();
  }

  std::unique_ptr<VideoCodecTestStats> stats_;
  TaskQueueForTest task_queue_;
  rtc::AutoThread thread_;
};

class PaceEncodeTest : public VideoCodecTesterImplTest,
                       public ::testing::WithParamInterface<bool> {};

TEST_P(PaceEncodeTest, PaceEncode) {
  // Video source delivers 1 frame per second. When the pacing is enabled,
  // encode calls must be spaced by at least 1 second.
  bool enable_pacing = GetParam();
  int64_t expected_time_delta = enable_pacing ? rtc::kNumNanosecsPerSec : 0;

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTime(Timestamp::Millis(123));

  auto video_source = std::make_unique<MockTestRawVideoSource>();
  EXPECT_CALL(*video_source, PullFrame)
      .WillOnce(Return(CreateVideoFrame(/*timestamp_rtp=*/0)))
      .WillOnce(Return(CreateVideoFrame(/*timestamp_rtp=*/90000)))
      .WillOnce(Return(absl::nullopt));

  auto encoder = std::make_unique<MockTestEncoder>();
  std::atomic_int encode_call_count = 0;
  ON_CALL(*encoder, Encode).WillByDefault([&encode_call_count] {
    ++encode_call_count;
  });

  VideoCodecTester::TestSettings test_settings;
  test_settings.realtime_encoding = enable_pacing;
  RunEncodeTest(std::move(video_source), test_settings, std::move(encoder));

  if (enable_pacing) {
    EXPECT_EQ_WAIT(1, encode_call_count, kWaitTimeout);
    fake_clock.AdvanceTime(TimeDelta::Millis(rtc::kNumMillisecsPerSec));
  }
  EXPECT_EQ_WAIT(2, encode_call_count, kWaitTimeout);

  auto fs = VideoCodecTesterImplTest::GetStats()->GetFrameStatistics();
  ASSERT_EQ(2u, fs.size());
  EXPECT_EQ(expected_time_delta, fs[1].encode_start_ns - fs[0].encode_start_ns);
}

INSTANTIATE_TEST_SUITE_P(VideoCodecTesterImplTest, PaceEncodeTest, Bool());

class PaceDecodeTest : public VideoCodecTesterImplTest,
                       public ::testing::WithParamInterface<bool> {};

TEST_P(PaceDecodeTest, PaceDecode) {
  // Video source delivers 1 frame per second. When the pacing is enabled,
  // decode calls must be spaced by at least 1 second.
  bool enable_pacing = GetParam();
  int64_t expected_time_delta = enable_pacing ? rtc::kNumNanosecsPerSec : 0;

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTime(Timestamp::Millis(123));

  auto video_source = std::make_unique<MockTestCodedVideoSource>();
  EXPECT_CALL(*video_source, PullFrame)
      .WillOnce(Return(CreateEncodedImage(/*timestamp_rtp=*/0)))
      .WillOnce(Return(CreateEncodedImage(/*timestamp_rtp=*/90000)))
      .WillOnce(Return(absl::nullopt));

  auto decoder = std::make_unique<MockTestDecoder>();
  std::atomic_int decode_call_count = 0;
  ON_CALL(*decoder, Decode).WillByDefault([&decode_call_count] {
    ++decode_call_count;
  });

  VideoCodecTester::TestSettings test_settings;
  test_settings.realtime_decoding = enable_pacing;
  RunDecodeTest(std::move(video_source), test_settings, std::move(decoder));

  if (enable_pacing) {
    EXPECT_EQ_WAIT(1, decode_call_count, kWaitTimeout);
    fake_clock.AdvanceTime(TimeDelta::Millis(rtc::kNumMillisecsPerSec));
  }
  EXPECT_EQ_WAIT(2, decode_call_count, kWaitTimeout);

  auto fs = VideoCodecTesterImplTest::GetStats()->GetFrameStatistics();
  ASSERT_EQ(2u, fs.size());
  EXPECT_EQ(expected_time_delta, fs[1].decode_start_ns - fs[0].decode_start_ns);
}

INSTANTIATE_TEST_SUITE_P(VideoCodecTesterImplTest, PaceDecodeTest, Bool());

}  // namespace test
}  // namespace webrtc
