/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_analyser.h"

#include "absl/types/optional.h"
#include "api/video/i420_buffer.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::Return;
using ::testing::Values;
using CodingSettings = VideoCodecAnalyser::CodingSettings;

const size_t kTimestamp = 3000;
const size_t kSpatialIdx = 2;

class MockVideoFrameReader : public VideoFrameReader {
 public:
  MOCK_METHOD(absl::optional<VideoFrame>,
              ReadFrame,
              (size_t frame_num),
              (override));
  MOCK_METHOD(void, Close, (), (override));
};

VideoFrame CreateVideoFrame(uint32_t timestamp_rtp,
                            uint8_t value_y = 0,
                            uint8_t value_u = 0,
                            uint8_t value_v = 0) {
  rtc::scoped_refptr<I420Buffer> buffer(I420Buffer::Create(2, 2));

  libyuv::I420Rect(
      buffer->MutableDataY(), buffer->StrideY(), buffer->MutableDataU(),
      buffer->StrideU(), buffer->MutableDataV(), buffer->StrideV(), 0, 0,
      buffer->width(), buffer->height(), value_y, value_u, value_v);

  return VideoFrame::Builder()
      .set_video_frame_buffer(buffer)
      .set_timestamp_rtp(timestamp_rtp)
      .build();
}

EncodedImage CreateEncodedImage(uint32_t timestamp_rtp, int spatial_idx = 0) {
  EncodedImage encoded_image;
  encoded_image.SetTimestamp(timestamp_rtp);
  encoded_image.SetSpatialIndex(spatial_idx);
  return encoded_image;
}
}  // namespace

TEST(VideoCodecAnalyserTest, EncodeStarted_createsFrameStats) {
  VideoCodecAnalyser analyser(
      /*frame_reader=*/nullptr);
  analyser.StartEncode(CreateVideoFrame(kTimestamp));

  auto stats = analyser.GetStats()->GetFrameStatistics();
  ASSERT_GT(stats.size(), 0u);

  VideoCodecTestStats::FrameStatistics fs = stats[0];
  EXPECT_EQ(fs.rtp_timestamp, kTimestamp);
}

TEST(PerfAnalyserScalabilityTest, EncodeFinished_updatesFrameStats) {
  VideoCodecAnalyser analyser(
      /*frame_reader=*/nullptr);
  analyser.StartEncode(CreateVideoFrame(kTimestamp));

  EncodedImage encoded_frame = CreateEncodedImage(kTimestamp, kSpatialIdx);
  CodingSettings settings;
  settings.bitrate_kbps = 1023;
  settings.framerate_fps = 21;
  analyser.FinishEncode(encoded_frame, settings);

  auto stats = analyser.GetStats()->GetFrameStatistics();
  ASSERT_GT(stats.size(), 0u);

  VideoCodecTestStats::FrameStatistics fs = stats[kSpatialIdx];
  EXPECT_TRUE(fs.encoding_successful);
  EXPECT_EQ(fs.target_bitrate_kbps, 1023u);
  EXPECT_EQ(fs.target_framerate_fps, 21);
}

TEST(VideoCodecAnalyserTest, DecodeStarted_noFrameStats_createsFrameStats) {
  VideoCodecAnalyser analyser(
      /*frame_reader=*/nullptr);
  analyser.StartDecode(CreateEncodedImage(kTimestamp, kSpatialIdx));

  auto stats = analyser.GetStats()->GetFrameStatistics();
  ASSERT_GT(stats.size(), 0u);

  VideoCodecTestStats::FrameStatistics fs = stats[kSpatialIdx];
  EXPECT_EQ(fs.rtp_timestamp, kTimestamp);
}

TEST(VideoCodecAnalyserTest, DecodeStarted_frameStatsExists_updatesFrameStats) {
  VideoCodecAnalyser analyser(
      /*frame_reader=*/nullptr);
  analyser.StartDecode(CreateEncodedImage(kTimestamp, kSpatialIdx));

  auto stats = analyser.GetStats()->GetFrameStatistics();
  ASSERT_GT(stats.size(), 0u);

  VideoCodecTestStats::FrameStatistics fs = stats[kSpatialIdx];
  EXPECT_NE(fs.decode_start_ns, 0);
}

TEST(VideoCodecAnalyserTest, DecodeFinished_updatesFrameStats) {
  VideoCodecAnalyser analyser(
      /*frame_reader=*/nullptr);
  analyser.StartDecode(CreateEncodedImage(kTimestamp, kSpatialIdx));
  VideoFrame decoded_frame = CreateVideoFrame(kTimestamp);
  analyser.FinishDecode(decoded_frame, kSpatialIdx);

  auto stats = analyser.GetStats()->GetFrameStatistics();
  ASSERT_GT(stats.size(), 0u);

  VideoCodecTestStats::FrameStatistics fs = stats[kSpatialIdx];
  EXPECT_TRUE(fs.decoding_successful);
  EXPECT_EQ(static_cast<int>(fs.decoded_width), decoded_frame.width());
  EXPECT_EQ(static_cast<int>(fs.decoded_height), decoded_frame.height());
}

TEST(VideoCodecAnalyserTest, DecodeFinished_computesPsnr) {
  MockVideoFrameReader frame_reader;
  VideoCodecAnalyser analyser(&frame_reader);
  analyser.StartDecode(CreateEncodedImage(kTimestamp, kSpatialIdx));

  EXPECT_CALL(frame_reader, ReadFrame(0))
      .WillOnce(Return(CreateVideoFrame(kTimestamp, /*value_y=*/0,
                                        /*value_u=*/0, /*value_v=*/0)));

  analyser.FinishDecode(
      CreateVideoFrame(kTimestamp, /*value_y=*/1, /*value_u=*/2, /*value_v=*/3),
      kSpatialIdx);

  auto stats = analyser.GetStats()->GetFrameStatistics();
  ASSERT_GT(stats.size(), 0u);

  VideoCodecTestStats::FrameStatistics fs = stats[kSpatialIdx];
  EXPECT_NEAR(fs.psnr_y, 48, 1);
  EXPECT_NEAR(fs.psnr_u, 42, 1);
  EXPECT_NEAR(fs.psnr_v, 38, 1);
}

}  // namespace test
}  // namespace webrtc
