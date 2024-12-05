/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/filter_settings_generator.h"

#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::AllOf;
using ::testing::DoubleEq;
using ::testing::DoubleNear;
using ::testing::Eq;
using ::testing::Field;

namespace webrtc {

TEST(FilterSettingsGenerator, ExponentialFunctionStdDev) {
  FilterSettingsGenerator fsg(
      FilterSettingsGenerator::ExponentialFunctionParameters{
          .scale = 0.006,
          .exponent_factor = 0.01857465,
          .exponent_offset = -4.26470513},
      FilterSettingsGenerator::ErrorThresholds{},
      webrtc::FilterSettingsGenerator::TransientParameters{});

  // 0.006 * e^(0.01857465 * 20 + 4.26470513) ~= 0.612
  CorruptionDetectionFilterSettings settings =
      fsg.OnFrame(/*is_keyframe=*/true, /*qp=*/20);
  EXPECT_THAT(settings.std_dev, DoubleNear(0.612, 0.01));

  // 0.006 * e^(0.01857465 * 20 + 4.26470513) ~= 1.886
  settings = fsg.OnFrame(/*is_keyframe=*/true, /*qp=*/80);
  EXPECT_THAT(settings.std_dev, DoubleNear(1.886, 0.01));
}

TEST(FilterSettingsGenerator, ExponentialFunctionThresholds) {
  FilterSettingsGenerator fsg(
      FilterSettingsGenerator::ExponentialFunctionParameters{
          .scale = 0.006,
          .exponent_factor = 0.01857465,
          .exponent_offset = -4.26470513},
      FilterSettingsGenerator::ErrorThresholds{.luma = 5, .chroma = 6},
      webrtc::FilterSettingsGenerator::TransientParameters{});

  CorruptionDetectionFilterSettings settings =
      fsg.OnFrame(/*is_keyframe=*/true, /*qp=*/20);
  EXPECT_EQ(settings.chroma_error_threshold, 6);
  EXPECT_EQ(settings.luma_error_threshold, 5);
}

TEST(FilterSettingsGenerator, RationalFunctionStdDev) {
  FilterSettingsGenerator fsg(
      FilterSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = -5.5, .denumerator_term = -97, .offset = -1},
      FilterSettingsGenerator::ErrorThresholds{},
      webrtc::FilterSettingsGenerator::TransientParameters{});

  // (20 * -5.5) / (20 - 97) - 1 ~= 0.429
  CorruptionDetectionFilterSettings settings =
      fsg.OnFrame(/*is_keyframe=*/true, /*qp=*/20);
  EXPECT_THAT(settings.std_dev, DoubleNear(0.429, 0.01));

  // (40 * -5.5) / (40 - 97) - 1 ~= 2.860
  settings = fsg.OnFrame(/*is_keyframe=*/true, /*qp=*/40);
  EXPECT_THAT(settings.std_dev, DoubleNear(2.860, 0.01));
}

TEST(FilterSettingsGenerator, RationalFunctionThresholds) {
  FilterSettingsGenerator fsg(
      FilterSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = -5.5, .denumerator_term = -97, .offset = -1},
      FilterSettingsGenerator::ErrorThresholds{.luma = 5, .chroma = 6},
      webrtc::FilterSettingsGenerator::TransientParameters{});

  CorruptionDetectionFilterSettings settings =
      fsg.OnFrame(/*is_keyframe=*/true, /*qp=*/20);
  EXPECT_EQ(settings.chroma_error_threshold, 6);
  EXPECT_EQ(settings.luma_error_threshold, 5);
}

TEST(FilterSettingsGenerator, TransientStdDevOffset) {
  FilterSettingsGenerator fsg(
      // (1 * qp) / (qp - 0) + 1 = 2, for all values of qp.
      FilterSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = 1, .denumerator_term = 0, .offset = 1},
      FilterSettingsGenerator::ErrorThresholds{},
      // Two frames with adjusted settings, including the keyframe.
      // Adjust the keyframe std_dev by 2.
      webrtc::FilterSettingsGenerator::TransientParameters{
          .keyframe_stddev_offset = 2.0,
          .keyframe_offset_duration_frames = 2,
      });

  EXPECT_THAT(fsg.OnFrame(/*is_keyframe=*/true, /*qp=*/1),
              Field(&CorruptionDetectionFilterSettings::std_dev,
                    DoubleNear(4.0, 0.001)));

  // Second frame has std_dev ofset interpolated halfway between keyframe
  // (2.0 + 2.0) and default (2.0) => 3.0
  EXPECT_THAT(fsg.OnFrame(/*is_keyframe=*/false, /*qp=*/1),
              Field(&CorruptionDetectionFilterSettings::std_dev,
                    DoubleNear(3.0, 0.001)));

  EXPECT_THAT(fsg.OnFrame(/*is_keyframe=*/false, /*qp=*/1),
              Field(&CorruptionDetectionFilterSettings::std_dev,
                    DoubleNear(2.0, 0.001)));

  EXPECT_THAT(fsg.OnFrame(/*is_keyframe=*/false, /*qp=*/1),
              Field(&CorruptionDetectionFilterSettings::std_dev,
                    DoubleNear(2.0, 0.001)));
}

TEST(FilterSettingsGenerator, TransientThresholdOffsets) {
  FilterSettingsGenerator fsg(
      FilterSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = 1, .denumerator_term = 0, .offset = 1},
      FilterSettingsGenerator::ErrorThresholds{.luma = 2, .chroma = 3},
      // Two frames with adjusted settings, including the keyframe.
      // Adjust the error thresholds by 2.
      webrtc::FilterSettingsGenerator::TransientParameters{
          .keyframe_threshold_offset = 2,
          .keyframe_offset_duration_frames = 2,
      });

  EXPECT_THAT(
      fsg.OnFrame(/*is_keyframe=*/true, /*qp=*/1),
      AllOf(Field(&CorruptionDetectionFilterSettings::chroma_error_threshold,
                  Eq(5)),
            Field(&CorruptionDetectionFilterSettings::luma_error_threshold,
                  Eq(4))));

  // Second frame has offset interpolated halfway between keyframe and default.
  EXPECT_THAT(
      fsg.OnFrame(/*is_keyframe=*/false, /*qp=*/1),
      AllOf(Field(&CorruptionDetectionFilterSettings::chroma_error_threshold,
                  Eq(4)),
            Field(&CorruptionDetectionFilterSettings::luma_error_threshold,
                  Eq(3))));

  EXPECT_THAT(
      fsg.OnFrame(/*is_keyframe=*/false, /*qp=*/1),
      AllOf(Field(&CorruptionDetectionFilterSettings::chroma_error_threshold,
                  Eq(3)),
            Field(&CorruptionDetectionFilterSettings::luma_error_threshold,
                  Eq(2))));

  EXPECT_THAT(
      fsg.OnFrame(/*is_keyframe=*/false, /*qp=*/1),
      AllOf(Field(&CorruptionDetectionFilterSettings::chroma_error_threshold,
                  Eq(3)),
            Field(&CorruptionDetectionFilterSettings::luma_error_threshold,
                  Eq(2))));
}

TEST(FilterSettingsGenerator, StdDevUpperBound) {
  FilterSettingsGenerator fsg(
      // (1 * qp) / (qp - 0) + 41 = 42, for all values of qp.
      FilterSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = 1, .denumerator_term = 0, .offset = 41},
      FilterSettingsGenerator::ErrorThresholds{},
      webrtc::FilterSettingsGenerator::TransientParameters{});

  // `std_dev` capped at max 40.0, which is the limit for the protocol.
  EXPECT_THAT(
      fsg.OnFrame(/*is_keyframe=*/true, /*qp=*/1),
      Field(&CorruptionDetectionFilterSettings::std_dev, DoubleEq(40.0)));
}

TEST(FilterSettingsGenerator, StdDevLowerBound) {
  FilterSettingsGenerator fsg(
      // (1 * qp) / (qp - 0) + 1 = 2, for all values of qp.
      FilterSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = 1, .denumerator_term = 0, .offset = 1},
      FilterSettingsGenerator::ErrorThresholds{},
      webrtc::FilterSettingsGenerator::TransientParameters{
          .std_dev_lower_bound = 5.0});

  // `std_dev` capped at lower bound of 5.0.
  EXPECT_THAT(
      fsg.OnFrame(/*is_keyframe=*/true, /*qp=*/1),
      Field(&CorruptionDetectionFilterSettings::std_dev, DoubleEq(5.0)));
}

TEST(FilterSettingsGenerator, TreatsLargeQpChangeAsKeyFrame) {
  FilterSettingsGenerator fsg(
      FilterSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = 1, .denumerator_term = 0, .offset = 1},
      FilterSettingsGenerator::ErrorThresholds{.luma = 2, .chroma = 3},
      // Two frames with adjusted settings, including the keyframe.
      // Adjust the error thresholds by 2.
      webrtc::FilterSettingsGenerator::TransientParameters{
          .max_qp = 100,
          .keyframe_threshold_offset = 2,
          .keyframe_offset_duration_frames = 1,
          .large_qp_change_threshold = 20});

  // +2 offset due to keyframe.
  EXPECT_THAT(
      fsg.OnFrame(/*is_keyframe=*/true, /*qp=*/10),
      Field(&CorruptionDetectionFilterSettings::luma_error_threshold, Eq(4)));

  // Back to normal.
  EXPECT_THAT(
      fsg.OnFrame(/*is_keyframe=*/false, /*qp=*/10),
      Field(&CorruptionDetectionFilterSettings::luma_error_threshold, Eq(2)));

  // Large change in qp, treat as keyframe => add +2 offset.
  EXPECT_THAT(
      fsg.OnFrame(/*is_keyframe=*/false, /*qp=*/30),
      Field(&CorruptionDetectionFilterSettings::luma_error_threshold, Eq(4)));

  // Back to normal.
  EXPECT_THAT(
      fsg.OnFrame(/*is_keyframe=*/false, /*qp=*/30),
      Field(&CorruptionDetectionFilterSettings::luma_error_threshold, Eq(2)));
}

}  // namespace webrtc
