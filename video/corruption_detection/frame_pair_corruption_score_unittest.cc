/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/frame_pair_corruption_score.h"

#include <memory>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace {

using test::FrameReader;

// Input video.
constexpr absl::string_view kFilename = "ConferenceMotion_1280_720_50";
constexpr int kWidth = 1280;
constexpr int kHeight = 720;

constexpr absl::string_view kCodecName = "VP8";

// Scale function parameters.
constexpr float kScaleFactor = 14;

// Logistic function parameters.
constexpr float kGrowthRate = 0.5;
constexpr float kMidpoint = 3;

std::unique_ptr<FrameReader> GetFrameGenerator() {
  std::string clip_path = test::ResourcePath(kFilename, "yuv");
  EXPECT_TRUE(test::FileExists(clip_path));
  return CreateYuvFrameReader(clip_path, {.width = kWidth, .height = kHeight},
                              test::YuvFrameReaderImpl::RepeatMode::kPingPong);
}

rtc::scoped_refptr<I420BufferInterface> GetDowscaledFrame(
    rtc::scoped_refptr<I420BufferInterface> frame,
    float downscale_factor) {
  rtc::scoped_refptr<I420Buffer> downscaled_frame =
      I420Buffer::Create(kWidth * downscale_factor, kHeight * downscale_factor);
  downscaled_frame->ScaleFrom(*frame);
  return downscaled_frame;
}

TEST(FramePairCorruptionScoreTest, SameFrameReturnsNoCorruptionScaleFunction) {
  std::unique_ptr<FrameReader> frame_reader = GetFrameGenerator();
  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();

  FramePairCorruptionScore frame_pair_corruption_score(kCodecName, kScaleFactor,
                                                       std::nullopt);
  EXPECT_LT(
      frame_pair_corruption_score.CalculateScore(/*qp=*/1, *frame, *frame),
      0.5);
}

TEST(FramePairCorruptionScoreTest,
     SameFrameReturnsNoCorruptionLogisticFunction) {
  std::unique_ptr<FrameReader> frame_reader = GetFrameGenerator();
  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();

  FramePairCorruptionScore frame_pair_corruption_score(kCodecName, kGrowthRate,
                                                       kMidpoint, std::nullopt);
  EXPECT_LT(
      frame_pair_corruption_score.CalculateScore(/*qp=*/1, *frame, *frame),
      0.5);
}

TEST(FramePairCorruptionScoreTest,
     HalfScaledFrameReturnsNoCorruptionScaleFunction) {
  std::unique_ptr<FrameReader> frame_reader = GetFrameGenerator();
  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();

  FramePairCorruptionScore frame_pair_corruption_score(kCodecName, kScaleFactor,
                                                       std::nullopt);
  EXPECT_LT(frame_pair_corruption_score.CalculateScore(
                /*qp=*/1, *frame,
                *GetDowscaledFrame(frame, /*downscale_factor=*/0.5)),
            0.5);
}

TEST(FramePairCorruptionScoreTest,
     HalfScaledFrameReturnsNoCorruptionLogisticFunction) {
  std::unique_ptr<FrameReader> frame_reader = GetFrameGenerator();
  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();

  FramePairCorruptionScore frame_pair_corruption_score(kCodecName, kGrowthRate,
                                                       kMidpoint, std::nullopt);
  EXPECT_LT(frame_pair_corruption_score.CalculateScore(
                /*qp=*/1, *frame,
                *GetDowscaledFrame(frame, /*downscale_factor=*/0.5)),
            0.5);
}

TEST(FramePairCorruptionScoreTest, QuarterScaledFrameReturnsNoCorruption) {
  std::unique_ptr<FrameReader> frame_reader = GetFrameGenerator();
  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();

  FramePairCorruptionScore frame_pair_corruption_score(kCodecName, kScaleFactor,
                                                       std::nullopt);
  EXPECT_LT(frame_pair_corruption_score.CalculateScore(
                /*qp=*/1, *frame,
                *GetDowscaledFrame(frame, /*downscale_factor=*/0.25)),
            0.5);
}

TEST(FramePairCorruptionScoreTest, WrongFrameResultsInCorruptionScaleFunction) {
  std::unique_ptr<FrameReader> frame_reader = GetFrameGenerator();
  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();

  // Get frame number 5, which should be different from the first frame, and
  // hence, indicate a corruption.
  rtc::scoped_refptr<I420Buffer> wrong_frame =
      frame_reader->ReadFrame(/*frame_num=*/5);

  FramePairCorruptionScore frame_pair_corruption_score(kCodecName, kScaleFactor,
                                                       std::nullopt);
  EXPECT_GT(frame_pair_corruption_score.CalculateScore(/*qp=*/1, *frame,
                                                       *wrong_frame),
            0.5);
}

TEST(FramePairCorruptionScoreTest,
     WrongFrameResultsInCorruptionLogisticFunction) {
  std::unique_ptr<FrameReader> frame_reader = GetFrameGenerator();
  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();

  // Get frame number 5, which should be different from the first frame, and
  // hence, indicate a corruption.
  rtc::scoped_refptr<I420Buffer> wrong_frame =
      frame_reader->ReadFrame(/*frame_num=*/5);

  FramePairCorruptionScore frame_pair_corruption_score(kCodecName, kGrowthRate,
                                                       kMidpoint, std::nullopt);
  EXPECT_GT(frame_pair_corruption_score.CalculateScore(/*qp=*/1, *frame,
                                                       *wrong_frame),
            0.5);
}

TEST(FramePairCorruptionScoreTest,
     HalfScaledWrongFrameResultsInCorruptionScaleFunction) {
  std::unique_ptr<FrameReader> frame_reader = GetFrameGenerator();
  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();

  // Get frame number 5, which should be different from the first frame, and
  // hence, indicate a corruption.
  rtc::scoped_refptr<I420Buffer> wrong_frame =
      frame_reader->ReadFrame(/*frame_num=*/5);

  FramePairCorruptionScore frame_pair_corruption_score(kCodecName, kScaleFactor,
                                                       std::nullopt);
  EXPECT_GT(frame_pair_corruption_score.CalculateScore(
                /*qp=*/1, *frame,
                *GetDowscaledFrame(wrong_frame, /*downscale_factor=*/0.25)),
            0.5);
}

TEST(FramePairCorruptionScoreTest,
     HalfScaledWrongFrameResultsInCorruptionLogisticFunction) {
  std::unique_ptr<FrameReader> frame_reader = GetFrameGenerator();
  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();

  // Get frame number 5, which should be different from the first frame, and
  // hence, indicate a corruption.
  rtc::scoped_refptr<I420Buffer> wrong_frame =
      frame_reader->ReadFrame(/*frame_num=*/5);

  FramePairCorruptionScore frame_pair_corruption_score(kCodecName, kGrowthRate,
                                                       kMidpoint, std::nullopt);
  EXPECT_GT(frame_pair_corruption_score.CalculateScore(
                /*qp=*/1, *frame,
                *GetDowscaledFrame(wrong_frame, /*downscale_factor=*/0.25)),
            0.5);
}

}  // namespace
}  // namespace webrtc
