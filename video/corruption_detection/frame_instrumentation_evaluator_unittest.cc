/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/frame_instrumentation_evaluator.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "common_video/corruption_detection_message.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

scoped_refptr<I420Buffer> MakeI420FrameBufferWithDifferentPixelValues() {
  // Create an I420 frame of size 4x4.
  const int kDefaultLumaWidth = 4;
  const int kDefaultLumaHeight = 4;
  const int kDefaultChromaWidth = 2;
  std::vector<uint8_t> kDefaultYContent = {1, 2,  3,  4,  5,  6,  7,  8,
                                           9, 10, 11, 12, 13, 14, 15, 16};
  std::vector<uint8_t> kDefaultUContent = {17, 18, 19, 20};
  std::vector<uint8_t> kDefaultVContent = {21, 22, 23, 24};

  return I420Buffer::Copy(kDefaultLumaWidth, kDefaultLumaHeight,
                          kDefaultYContent.data(), kDefaultLumaWidth,
                          kDefaultUContent.data(), kDefaultChromaWidth,
                          kDefaultVContent.data(), kDefaultChromaWidth);
}

TEST(FrameInstrumentationEvaluatorTest,
     HaveNoCorruptionScoreWhenNoSampleValuesAreProvided) {
  std::optional<CorruptionDetectionMessage> message =
      CorruptionDetectionMessage::Builder().Build();
  ASSERT_TRUE(message.has_value());
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();
  FrameInstrumentationEvaluator frame_instrumentation_evaluator;

  std::optional<double> corruption_score =
      frame_instrumentation_evaluator.GetCorruptionScore(*message, frame);

  EXPECT_FALSE(corruption_score.has_value());
}

TEST(FrameInstrumentationEvaluatorTest,
     HaveACorruptionScoreWhenSampleValuesAreProvided) {
  std::vector<double> sample_values = {1};
  std::optional<CorruptionDetectionMessage> message =
      CorruptionDetectionMessage::Builder()
          .WithSampleValues(sample_values)
          .Build();
  ASSERT_TRUE(message.has_value());
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();
  FrameInstrumentationEvaluator frame_instrumentation_evaluator;

  std::optional<double> corruption_score =
      frame_instrumentation_evaluator.GetCorruptionScore(*message, frame);

  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_DOUBLE_EQ(*corruption_score, 0.0);
}

TEST(FrameInstrumentationEvaluatorTest,
     ApplyThresholdsWhenNonNegativeThresholdsAreProvided) {
  std::vector<double> sample_values = {12, 12, 12, 12, 12, 12, 12, 12};
  std::optional<CorruptionDetectionMessage> message =
      CorruptionDetectionMessage::Builder()
          .WithSampleValues(sample_values)
          .WithLumaErrorThreshold(8)
          .WithChromaErrorThreshold(8)
          .Build();
  ASSERT_TRUE(message.has_value());
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();
  FrameInstrumentationEvaluator frame_instrumentation_evaluator;

  std::optional<double> corruption_score =
      frame_instrumentation_evaluator.GetCorruptionScore(*message, frame);

  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_DOUBLE_EQ(*corruption_score, 0.55);
}

TEST(FrameInstrumentationEvaluatorTest,
     ApplyStdDevWhenNonNegativeStdDevIsProvided) {
  std::vector<double> sample_values = {12, 12, 12, 12, 12, 12, 12, 12};
  std::optional<CorruptionDetectionMessage> message =
      CorruptionDetectionMessage::Builder()
          .WithSampleValues(sample_values)
          .WithLumaErrorThreshold(8)
          .WithChromaErrorThreshold(8)
          .WithStdDev(0.6)
          .Build();
  ASSERT_TRUE(message.has_value());
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();
  FrameInstrumentationEvaluator frame_instrumentation_evaluator;

  std::optional<double> corruption_score =
      frame_instrumentation_evaluator.GetCorruptionScore(*message, frame);

  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_DOUBLE_EQ(*corruption_score, 0.3302493109581533);
}

TEST(FrameInstrumentationEvaluatorTest,
     UpdateSequenceIndexWhenMessageDefinesOne) {
  std::vector<double> sample_values = {12, 12, 12, 12, 12, 12, 12, 12};
  std::optional<CorruptionDetectionMessage> message1 =
      CorruptionDetectionMessage::Builder()
          .WithSampleValues(sample_values)
          .WithLumaErrorThreshold(8)
          .WithChromaErrorThreshold(8)
          .WithStdDev(0.7)
          .Build();
  ASSERT_TRUE(message1.has_value());
  std::optional<CorruptionDetectionMessage> message2 =
      CorruptionDetectionMessage::Builder()
          .WithSampleValues(sample_values)
          .WithLumaErrorThreshold(8)
          .WithChromaErrorThreshold(8)
          .WithStdDev(0.7)
          .WithSequenceIndex(15)  // 8 -> 15
          .WithInterpretSequenceIndexAsMostSignificantBits(false)
          .Build();
  ASSERT_TRUE(message2.has_value());
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();
  FrameInstrumentationEvaluator frame_instrumentation_evaluator;
  std::optional<double> corruption_score =
      frame_instrumentation_evaluator.GetCorruptionScore(*message1, frame);
  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_DOUBLE_EQ(*corruption_score, 0.30139808164602827);

  corruption_score =
      frame_instrumentation_evaluator.GetCorruptionScore(*message2, frame);

  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_DOUBLE_EQ(*corruption_score, 0.2564994501432018);
}

TEST(FrameInstrumentationEvaluatorTest,
     WraparoundSequenceIndexWhenMessageLowerBitsLessAreLessThanCurrentIndex) {
  std::vector<double> sample_values = {12, 12, 12, 12, 12, 12, 12, 12};
  std::optional<CorruptionDetectionMessage> message1 =
      CorruptionDetectionMessage::Builder()
          .WithSampleValues(sample_values)
          .WithLumaErrorThreshold(8)
          .WithChromaErrorThreshold(8)
          .WithStdDev(0.7)
          .Build();
  ASSERT_TRUE(message1.has_value());
  std::optional<CorruptionDetectionMessage> message2 =
      CorruptionDetectionMessage::Builder()
          .WithSampleValues(sample_values)
          .WithLumaErrorThreshold(8)
          .WithChromaErrorThreshold(8)
          .WithStdDev(0.7)
          .WithSequenceIndex(0)  // 8 -> 128
          .WithInterpretSequenceIndexAsMostSignificantBits(false)
          .Build();
  ASSERT_TRUE(message2.has_value());
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();
  FrameInstrumentationEvaluator frame_instrumentation_evaluator;
  std::optional<double> corruption_score =
      frame_instrumentation_evaluator.GetCorruptionScore(*message1, frame);
  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_DOUBLE_EQ(*corruption_score, 0.30139808164602827);

  corruption_score =
      frame_instrumentation_evaluator.GetCorruptionScore(*message2, frame);

  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_DOUBLE_EQ(*corruption_score, 0.55785093326006396);
}

TEST(FrameInstrumentationEvaluatorTest,
     SetSequenceIndexToTheMessagesWhenUpdateUpperBits) {
  std::vector<double> sample_values = {12, 12, 12, 12, 12, 12, 12, 12};
  std::optional<CorruptionDetectionMessage> message1 =
      CorruptionDetectionMessage::Builder()
          .WithSampleValues(sample_values)
          .WithLumaErrorThreshold(8)
          .WithChromaErrorThreshold(8)
          .WithStdDev(0.7)
          .Build();
  ASSERT_TRUE(message1.has_value());
  std::optional<CorruptionDetectionMessage> message2 =
      CorruptionDetectionMessage::Builder()
          .WithSampleValues(sample_values)
          .WithLumaErrorThreshold(8)
          .WithChromaErrorThreshold(8)
          .WithStdDev(0.7)
          .WithSequenceIndex(1)  // 8 -> 128
          .WithInterpretSequenceIndexAsMostSignificantBits(true)
          .Build();
  ASSERT_TRUE(message1.has_value());
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .build();
  FrameInstrumentationEvaluator frame_instrumentation_evaluator;
  std::optional<double> corruption_score =
      frame_instrumentation_evaluator.GetCorruptionScore(*message1, frame);
  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_DOUBLE_EQ(*corruption_score, 0.30139808164602827);

  corruption_score =
      frame_instrumentation_evaluator.GetCorruptionScore(*message2, frame);

  ASSERT_TRUE(corruption_score.has_value());
  EXPECT_DOUBLE_EQ(*corruption_score, 0.55785093326006396);
}

}  // namespace
}  // namespace webrtc
