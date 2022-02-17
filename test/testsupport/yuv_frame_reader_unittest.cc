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
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

namespace {
const std::vector<std::string> kFrameContents = {"012345", "123456", "234567"};

const size_t kFrameWidth = 2;
const size_t kFrameHeight = 2;
const size_t kFrameLength = 3 * kFrameWidth * kFrameHeight / 2;  // I420.
}  // namespace

class YuvFrameReaderTest : public ::testing::Test {
 protected:
  YuvFrameReaderTest() = default;
  ~YuvFrameReaderTest() override = default;

  void SetUp() override {
    temp_filename_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                                "yuv_frame_reader_unittest");
    FILE* dummy = fopen(temp_filename_.c_str(), "wb");
    for (const std::string& frame : kFrameContents) {
      fprintf(dummy, "%s", frame.c_str());
    }
    fclose(dummy);

    frame_reader_.reset(
        new YuvFrameReaderImpl(temp_filename_, kFrameWidth, kFrameHeight));
    ASSERT_TRUE(frame_reader_->Init());
  }

  void TearDown() override { remove(temp_filename_.c_str()); }

  std::unique_ptr<FrameReader> frame_reader_;
  std::string temp_filename_;
};

TEST_F(YuvFrameReaderTest, InitSuccess) {}

TEST_F(YuvFrameReaderTest, FrameLength) {
  EXPECT_EQ(kFrameLength, frame_reader_->FrameLength());
}

TEST_F(YuvFrameReaderTest, NumberOfFrames) {
  EXPECT_EQ(static_cast<int>(kFrameContents.size()),
            frame_reader_->NumberOfFrames());
}

TEST_F(YuvFrameReaderTest, ReadFrameUninitialized) {
  YuvFrameReaderImpl file_reader(temp_filename_, kFrameWidth, kFrameHeight);
  EXPECT_FALSE(file_reader.ReadFrame());
}

TEST_F(YuvFrameReaderTest, ReadFrame) {
  for (const std::string& expected_frame : kFrameContents) {
    rtc::scoped_refptr<I420BufferInterface> buffer = frame_reader_->ReadFrame();
    ASSERT_TRUE(buffer);
    EXPECT_EQ(expected_frame[0], buffer->DataY()[0]);
    EXPECT_EQ(expected_frame[1], buffer->DataY()[1]);
    EXPECT_EQ(expected_frame[2], buffer->DataY()[2]);
    EXPECT_EQ(expected_frame[3], buffer->DataY()[3]);
    EXPECT_EQ(expected_frame[4], buffer->DataU()[0]);
    EXPECT_EQ(expected_frame[5], buffer->DataV()[0]);
  }
  EXPECT_FALSE(frame_reader_->ReadFrame());  // End of file.
}

TEST_F(YuvFrameReaderTest, RepeatMode) {
  frame_reader_.reset(new YuvFrameReaderImpl(
      temp_filename_, kFrameWidth, kFrameHeight, kFrameWidth, kFrameHeight,
      YuvFrameReaderImpl::RepeatMode::kRepeat, absl::nullopt, 30));
  ASSERT_TRUE(frame_reader_->Init());

  for (int i = 0; i < 2; ++i) {
    for (const std::string& expected_frame : kFrameContents) {
      rtc::scoped_refptr<I420BufferInterface> buffer =
          frame_reader_->ReadFrame();
      ASSERT_TRUE(buffer);
      EXPECT_EQ(expected_frame[0], buffer->DataY()[0]);
      EXPECT_EQ(expected_frame[1], buffer->DataY()[1]);
      EXPECT_EQ(expected_frame[2], buffer->DataY()[2]);
      EXPECT_EQ(expected_frame[3], buffer->DataY()[3]);
      EXPECT_EQ(expected_frame[4], buffer->DataU()[0]);
      EXPECT_EQ(expected_frame[5], buffer->DataV()[0]);
    }
  }
}

TEST_F(YuvFrameReaderTest, PingPongMode) {
  frame_reader_.reset(new YuvFrameReaderImpl(
      temp_filename_, kFrameWidth, kFrameHeight, kFrameWidth, kFrameHeight,
      YuvFrameReaderImpl::RepeatMode::kPingPong, absl::nullopt, 30));
  ASSERT_TRUE(frame_reader_->Init());

  const std::vector<int> expected_frame_index = {0, 1, 2, 1, 0, 1, 2};
  for (const int expected_index : expected_frame_index) {
    rtc::scoped_refptr<I420BufferInterface> buffer = frame_reader_->ReadFrame();
    ASSERT_TRUE(buffer);
    EXPECT_EQ(kFrameContents[expected_index][0], buffer->DataY()[0]);
    EXPECT_EQ(kFrameContents[expected_index][1], buffer->DataY()[1]);
    EXPECT_EQ(kFrameContents[expected_index][2], buffer->DataY()[2]);
    EXPECT_EQ(kFrameContents[expected_index][3], buffer->DataY()[3]);
    EXPECT_EQ(kFrameContents[expected_index][4], buffer->DataU()[0]);
    EXPECT_EQ(kFrameContents[expected_index][5], buffer->DataV()[0]);
  }
}

}  // namespace test
}  // namespace webrtc
