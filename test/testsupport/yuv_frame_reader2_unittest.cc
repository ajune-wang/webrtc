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
#include <stdio.h>

#include <memory>
#include <string>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/yuv_frame_reader.h"

namespace webrtc {
namespace test {

namespace {
using YuvFrameReader::RepeatMode::kPingPong;
using YuvFrameReader::RepeatMode::kRepeat;

constexpr int kWidth = 1;
constexpr int kHeight = 1;
constexpr int kFrameSizeBytes =
    kWidth * kHeight + 2 * ((kHeight + 1) / 2) * ((kHeight + 1) / 2);
constexpr char kFrameContent[3][3] = {{0, 1, 2}, {1, 2, 3}, {2, 3, 4}};
constexpr int kNumFrames = sizeof(kFrameContent) / sizeof(kFrameContent[0]);
}  // namespace

class YuvFrameReader2Test : public ::testing::Test {
 protected:
  YuvFrameReader2Test() = default;
  ~YuvFrameReader2Test() override = default;

  void SetUp() override {
    filepath_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                           "yuv_frame_reader_unittest");
    FILE* file = fopen(filepath_.c_str(), "wb");
    fwrite(kFrameContent, sizeof(char), sizeof(kFrameContent), file);
    fclose(file);
  }

  void TearDown() override { remove(filepath_.c_str()); }

  std::string filepath_;
};

TEST_F(YuvFrameReader2Test, FrameSizeBytes) {
  auto reader = CreateYuvFrameReader(filepath_, kWidth, kHeight);
  EXPECT_EQ(kFrameSizeBytes, reader->FrameSizeBytes());
}

TEST_F(YuvFrameReader2Test, NumberOfFrames) {
  auto reader = CreateYuvFrameReader(filepath_, kWidth, kHeight);
  EXPECT_EQ(kNumFrames, reader->NumberOfFrames());
}

TEST_F(YuvFrameReader2Test, PullFrame_frameContent) {
  auto reader = CreateYuvFrameReader(filepath_, kWidth, kHeight);
  rtc::scoped_refptr<I420BufferInterface> buffer = reader->PullFrame();
  EXPECT_EQ(kFrameContent[0][0], *buffer->DataY());
  EXPECT_EQ(kFrameContent[0][1], *buffer->DataU());
  EXPECT_EQ(kFrameContent[0][2], *buffer->DataV());
}

TEST_F(YuvFrameReader2Test, ReadFrame_randomOrder) {
  std::vector<int> expected_frames = {2, 0, 1};
  auto reader = CreateYuvFrameReader(filepath_, kWidth, kHeight);
  std::vector<int> actual_frames;
  for (int frame_num : expected_frames) {
    rtc::scoped_refptr<I420BufferInterface> buffer =
        reader->ReadFrame(frame_num);
    actual_frames.push_back(*buffer->DataY());
  }
  EXPECT_EQ(expected_frames, actual_frames);
}

TEST_F(YuvFrameReader2Test, PullFrame_scale) {
  auto reader = CreateYuvFrameReader(filepath_, kWidth, kHeight);
  rtc::scoped_refptr<I420BufferInterface> buffer = reader->PullFrame(
      /*pulled_frame_num=*/nullptr,
      /*desired_width=*/2,
      /*desired_height=*/2, /*base_framerate=*/1, /*desired_framerate=*/1);
  EXPECT_EQ(2, buffer->width());
  EXPECT_EQ(2, buffer->height());
}

class RepeatModeTest
    : public YuvFrameReader2Test,
      public ::testing::WithParamInterface<
          std::tuple<YuvFrameReader::RepeatMode, std::vector<int>>> {};

TEST_P(RepeatModeTest, PullFrame) {
  YuvFrameReader::RepeatMode mode = std::get<0>(GetParam());
  std::vector<int> expected_frames = std::get<1>(GetParam());

  auto reader = CreateYuvFrameReader(filepath_, kWidth, kHeight, mode);
  std::vector<int> read_frames;
  for (size_t i = 0; i < expected_frames.size(); ++i) {
    rtc::scoped_refptr<I420BufferInterface> buffer = reader->PullFrame();
    read_frames.push_back(*buffer->DataY());
  }
  EXPECT_EQ(expected_frames, read_frames);
}

INSTANTIATE_TEST_SUITE_P(
    YuvFrameReader2Test,
    RepeatModeTest,
    ::testing::ValuesIn(
        {std::make_tuple(kRepeat, std::vector<int>{0, 1, 2, 0, 1, 2}),
         std::make_tuple(kPingPong, std::vector<int>{0, 1, 2, 1, 0, 1, 2})}));

class RateScaleTest : public YuvFrameReader2Test,
                      public ::testing::WithParamInterface<
                          std::tuple<int, int, std::vector<int>>> {};

TEST_P(RateScaleTest, PullFrame) {
  int base_rate = std::get<0>(GetParam());
  int target_rate = std::get<1>(GetParam());
  std::vector<int> expected_frames = std::get<2>(GetParam());

  auto reader = CreateYuvFrameReader(filepath_, kWidth, kHeight, kRepeat);
  std::vector<int> actual_frames;
  for (size_t i = 0; i < expected_frames.size(); ++i) {
    int pulled_frame;
    rtc::scoped_refptr<I420BufferInterface> buffer = reader->PullFrame(
        &pulled_frame, kWidth, kHeight, base_rate, target_rate);
    actual_frames.push_back(pulled_frame);
  }
  EXPECT_EQ(expected_frames, actual_frames);
}

INSTANTIATE_TEST_SUITE_P(
    YuvFrameReader2Test,
    RateScaleTest,
    ::testing::ValuesIn(
        {std::make_tuple(2, 1, std::vector<int>{0, 2, 4}),
         std::make_tuple(3, 2, std::vector<int>{0, 2, 3, 5, 6})}));
}  // namespace test
}  // namespace webrtc
