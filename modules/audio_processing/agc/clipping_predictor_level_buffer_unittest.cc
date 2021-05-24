/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/clipping_predictor_level_buffer.h"

#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int kBufferLength = 10;
constexpr float kMaxErrorAllowed = 0.00001f;

}  // namespace

class ClippingPredictorLevelBufferTest : public ::testing::Test {
 protected:
  ClippingPredictorLevelBufferTest() {}

  void PopulateBuffer(int num_values, ClippingPredictorLevelBuffer& buffer) {
    for (int i = 0; i < num_values; ++i) {
      const float average_value = static_cast<float>(i) / 10.f + 0.1f;
      const float max_value = static_cast<float>(i) + 1.f;
      buffer.Push({average_value, max_value});
    }
  }
};

TEST_F(ClippingPredictorLevelBufferTest,
       ProcessingIncompleteBufferSuccessfull) {
  const int num_values = 4;
  ClippingPredictorLevelBuffer buffer(kBufferLength);
  PopulateBuffer(num_values, buffer);
  EXPECT_EQ(buffer.Size(), num_values);
  const float expect_max[] = {4.f, 3.f, 2.f, 1.f};
  const float expect_avg[] = {0.25f, 0.2f, 0.15f, 0.1f};
  const float expect_value[] = {0.4f, 0.3f, 0.2f, 0.1f};
  for (int delay = 0; delay < num_values; ++delay) {
    SCOPED_TRACE(delay);
    auto level = *buffer.ComputePartialMetrics(delay, num_values - delay);
    EXPECT_EQ(level.max, expect_max[delay]);
    EXPECT_NEAR(level.average, expect_avg[delay], kMaxErrorAllowed);
    auto value = *buffer.ComputePartialMetrics(delay, /*num_items*/ 1);
    EXPECT_NEAR(value.average, expect_value[delay], kMaxErrorAllowed);
    EXPECT_EQ(value.max, expect_max[delay]);
  }
  for (int delay = num_values; delay < kBufferLength; ++delay) {
    SCOPED_TRACE(delay);
    for (int num_items = 1; num_items < kBufferLength - delay; ++num_items) {
      SCOPED_TRACE(num_items);
      EXPECT_FALSE(buffer.ComputePartialMetrics(delay, num_items));
    }
  }
}

TEST_F(ClippingPredictorLevelBufferTest, FirstProcessingSuccessfull) {
  const int num_values = kBufferLength;
  ClippingPredictorLevelBuffer buffer(kBufferLength);
  PopulateBuffer(num_values, buffer);
  EXPECT_EQ(buffer.Size(), num_values);
  const float expect_max[] = {10.f, 9.f, 8.f, 7.f, 6.f,
                              5.f,  4.f, 3.f, 2.f, 1.f};
  const float expect_avg[] = {0.55f, 0.5f,  0.45f, 0.4f,  0.35f,
                              0.3f,  0.25f, 0.2f,  0.15f, 0.1f};
  const float expect_value[] = {1.f,  0.9f, 0.8f, 0.7f, 0.6f,
                                0.5f, 0.4f, 0.3f, 0.2f, 0.1f};
  for (int delay = 0; delay < 10; ++delay) {
    SCOPED_TRACE(delay);
    const int num_items = kBufferLength - delay;
    const auto level = *buffer.ComputePartialMetrics(delay, num_items);
    EXPECT_NEAR(level.max, expect_max[delay], kMaxErrorAllowed);
    EXPECT_NEAR(level.average, expect_avg[delay], kMaxErrorAllowed);
    const auto value = *buffer.ComputePartialMetrics(delay, /*num_items*/ 1);
    EXPECT_NEAR(value.max, expect_max[delay], kMaxErrorAllowed);
    EXPECT_NEAR(value.average, expect_value[delay], kMaxErrorAllowed);
  }
  const float expect_avg_short[] = {0.9f, 0.8f, 0.7f, 0.6f,
                                    0.5f, 0.4f, 0.3f, 0.2f};
  for (int delay = 0; delay < 8; ++delay) {
    SCOPED_TRACE(delay);
    const auto level = *buffer.ComputePartialMetrics(delay, /*num_items*/ 3);
    EXPECT_NEAR(level.average, expect_avg_short[delay], kMaxErrorAllowed);
    EXPECT_NEAR(level.max, expect_max[delay], kMaxErrorAllowed);
  }
}

TEST_F(ClippingPredictorLevelBufferTest, RepeatedProcessingSuccessfull) {
  const int num_values = kBufferLength + 4;
  ClippingPredictorLevelBuffer buffer(kBufferLength);
  PopulateBuffer(num_values, buffer);
  auto level = *buffer.ComputePartialMetrics(0, 1);
  EXPECT_EQ(level.max, 14.f);
  EXPECT_NEAR(level.average, 1.4f, kMaxErrorAllowed);
  level = *buffer.ComputePartialMetrics(1, 4);
  EXPECT_EQ(level.max, 13.f);
  EXPECT_NEAR(level.average, 1.15f, kMaxErrorAllowed);
  level = *buffer.ComputePartialMetrics(9, 1);
  EXPECT_EQ(level.max, 5.f);
  EXPECT_NEAR(level.average, 0.5f, kMaxErrorAllowed);
  level = *buffer.ComputePartialMetrics(2, 8);
  EXPECT_EQ(level.max, 12.f);
  EXPECT_NEAR(level.average, 0.85f, kMaxErrorAllowed);
  // Add more items without emptying the buffer. Buffer size is not increased,
  // previously added values are taken into account in calculations.
  PopulateBuffer(/*num_values*/ 2, buffer);
  EXPECT_EQ(buffer.Size(), kBufferLength);
  level = *buffer.ComputePartialMetrics(0, 1);
  EXPECT_EQ(level.max, 2.f);
  EXPECT_NEAR(level.average, 0.2f, kMaxErrorAllowed);
  level = *buffer.ComputePartialMetrics(1, 1);
  EXPECT_EQ(level.max, 1.f);
  EXPECT_NEAR(level.average, 0.1f, kMaxErrorAllowed);
  level = *buffer.ComputePartialMetrics(0, 2);
  EXPECT_EQ(level.max, 2.f);
  EXPECT_NEAR(level.average, 0.15f, kMaxErrorAllowed);
  level = *buffer.ComputePartialMetrics(0, kBufferLength);
  EXPECT_EQ(level.max, 14.f);
  EXPECT_NEAR(level.average, 0.87f, kMaxErrorAllowed);
  // Add more items without emptying the buffer.
  PopulateBuffer(2 * kBufferLength + 4, buffer);
  EXPECT_EQ(buffer.Size(), kBufferLength);
  level = *buffer.ComputePartialMetrics(0, 1);
  EXPECT_EQ(level.max, 24.f);
  EXPECT_NEAR(level.average, 2.4f, kMaxErrorAllowed);
  level = *buffer.ComputePartialMetrics(1, 4);
  EXPECT_EQ(level.max, 23.f);
  EXPECT_NEAR(level.average, 2.15f, kMaxErrorAllowed);
  level = *buffer.ComputePartialMetrics(2, 8);
  EXPECT_EQ(level.max, 22.f);
  EXPECT_NEAR(level.average, 1.85f, kMaxErrorAllowed);
}

TEST_F(ClippingPredictorLevelBufferTest, NoMetricsForNearlyEmptyBuffer) {
  const int num_values = 4;
  ClippingPredictorLevelBuffer buffer(kBufferLength);
  PopulateBuffer(num_values, buffer);
  for (int delay = 0; delay < kBufferLength; ++delay) {
    for (int length = 1; length < kBufferLength - delay; ++length) {
      SCOPED_TRACE(delay);
      if (delay + length <= num_values) {
        EXPECT_TRUE(buffer.ComputePartialMetrics(delay, length));
      } else {
        EXPECT_FALSE(buffer.ComputePartialMetrics(delay, length));
      }
    }
  }
  PopulateBuffer(kBufferLength, buffer);
  for (int delay = 0; delay < kBufferLength; ++delay) {
    for (int length = 1; length < kBufferLength - delay; ++length) {
      SCOPED_TRACE(delay);
      EXPECT_TRUE(buffer.ComputePartialMetrics(delay, length));
    }
  }
}

TEST_F(ClippingPredictorLevelBufferTest, NoMetricsForEmptyBuffer) {
  const int num_values = 0;
  ClippingPredictorLevelBuffer buffer(kBufferLength);
  PopulateBuffer(num_values, buffer);
  EXPECT_EQ(buffer.Size(), num_values);
  for (int i = 0; i < kBufferLength; ++i) {
    for (int j = 1; j < kBufferLength - i; ++j) {
      SCOPED_TRACE(i);
      EXPECT_FALSE(buffer.ComputePartialMetrics(i, j));
    }
  }
}

}  // namespace webrtc
