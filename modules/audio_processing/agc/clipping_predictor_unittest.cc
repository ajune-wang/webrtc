/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/clipping_predictor.h"

#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

const int kSampleRateHz = 32000;
const int kNumChannels = 1;
const int kSamplesPerChannel = kSampleRateHz / 100;
const size_t kNumFrames = 5;
const size_t kNumPreviousFrames = 5;
const size_t kBufferSize = 10;

}  // namespace

class ClippingPredictorTest : public ::testing::Test {
 protected:
  ClippingPredictorTest()
      : predictor_(kNumChannels, 5, 5, -1, 3),
        audio(kNumChannels),
        audio_data(kNumChannels * kSamplesPerChannel, 0.f) {
    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      audio[ch] = &audio_data[ch * kSamplesPerChannel];
    }
  }

  bool ProcessSimpleAudio(float peak_ratio) {
    RTC_DCHECK_GE(1.f, peak_ratio);
    std::fill(audio_data.begin(), audio_data.end(), 0.f);
    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      for (int k = 0; k < kSamplesPerChannel; ++k) {
        audio[ch][k] = peak_ratio * 32767.f;
      }
    }
    predictor_.ProcessAudioFrame(audio.data(), kNumChannels,
                                 kSamplesPerChannel);
    return predictor_.PredictClippingEvent(/*channel*/ 0);
  }

  bool ProcessNonZeroCrestFactor(float peak_ratio) {
    RTC_DCHECK_GE(1.f, peak_ratio);
    bool clipping_predicted = false;
    for (size_t i = 0; i < 2 * kNumFrames; ++i) {
      std::fill(audio_data.begin(), audio_data.end(), 0.f);
      for (size_t ch = 0; ch < kNumChannels; ++ch) {
        for (int k = 0; k < kSamplesPerChannel; k += 2) {
          audio[ch][k] = peak_ratio * 32767.f;
        }
      }
      predictor_.ProcessAudioFrame(audio.data(), kNumChannels,
                                   kSamplesPerChannel);
      clipping_predicted =
          clipping_predicted || predictor_.PredictClippingEvent(/*channel*/ 0);
    }
    return clipping_predicted;
  }

  bool ProcessZeroCrestFactor(float peak_ratio) {
    RTC_DCHECK_GE(1.f, peak_ratio);
    bool clipping_predicted = false;
    for (size_t i = 0; i < 2 * kNumFrames; ++i) {
      std::fill(audio_data.begin(), audio_data.end(), 0.f);
      for (size_t ch = 0; ch < kNumChannels; ++ch) {
        for (int k = 0; k < kSamplesPerChannel; ++k) {
          audio[ch][k] = peak_ratio * 32767.f;
        }
      }
      predictor_.ProcessAudioFrame(audio.data(), kNumChannels,
                                   kSamplesPerChannel);
      clipping_predicted =
          clipping_predicted || predictor_.PredictClippingEvent(/*channel*/ 0);
    }
    return clipping_predicted;
  }

  ClippingPredictor predictor_;
  std::vector<float*> audio;
  std::vector<float> audio_data;
};

TEST_F(ClippingPredictorTest, ClippingNotPredicted) {
  predictor_.Reset();
  EXPECT_FALSE(ProcessSimpleAudio(1.0f));
  predictor_.Reset();
  EXPECT_FALSE(ProcessSimpleAudio(0.6f));
  EXPECT_FALSE(ProcessSimpleAudio(0.99f));
}

TEST_F(ClippingPredictorTest, ClippingPredictedForHighVolume) {
  predictor_.Reset();
  EXPECT_FALSE(ProcessNonZeroCrestFactor(0.99f));
  EXPECT_TRUE(ProcessZeroCrestFactor(0.99f));
  EXPECT_FALSE(ProcessZeroCrestFactor(0.99f));
}

TEST_F(ClippingPredictorTest, ClippingPredictedForIncreasedVolume) {
  predictor_.Reset();
  EXPECT_FALSE(ProcessNonZeroCrestFactor(0.6f));
  EXPECT_FALSE(ProcessZeroCrestFactor(0.6f));
  EXPECT_TRUE(ProcessZeroCrestFactor(0.99f));
}

TEST_F(ClippingPredictorTest, ClippingNotPredictedForLowVolume) {
  predictor_.Reset();
  EXPECT_FALSE(ProcessNonZeroCrestFactor(0.6));
  EXPECT_FALSE(ProcessZeroCrestFactor(0.6));
  EXPECT_FALSE(ProcessNonZeroCrestFactor(0.6));
}

class LevelBufferTest : public ::testing::Test {
 protected:
  LevelBufferTest() : buffer_(kBufferSize) {}

  void FillBuffer(const size_t num_items) {
    buffer_.Reset();
    for (size_t i = 0; i < num_items; ++i) {
      buffer_.Push(
          {static_cast<float>(i + 1) / 10.f, static_cast<float>(i + 1)});
    }
  }

  LevelBuffer buffer_;
};

TEST_F(LevelBufferTest, ProcessingIncompleteBufferSuccessfull) {
  const size_t buffer_size = 4;
  FillBuffer(buffer_size);
  EXPECT_EQ(buffer_.Size(), buffer_size);
  const float expect_value[] = {4.f, 3.f, 2.f, 1.f};
  const float expect_avg[] = {0.25f, 0.2f, 0.15f, 0.1f};
  for (size_t delay = 0; delay < 4; ++delay) {
    const size_t num_items = buffer_size - delay;
    EXPECT_EQ(buffer_.ComputePartialMax(delay, num_items), expect_value[delay]);
    EXPECT_EQ(buffer_.ComputePartialAverage(delay, num_items),
              expect_avg[delay]);
    EXPECT_EQ(buffer_.ComputePartialMax(delay, 1), expect_value[delay]);
    EXPECT_EQ(buffer_.ComputePartialAverage(delay, 1),
              expect_value[delay] / 10.f);
  }
}

TEST_F(LevelBufferTest, FirstProcessingSuccessfull) {
  const size_t buffer_size = 10;
  FillBuffer(buffer_size);
  EXPECT_EQ(buffer_.Size(), buffer_size);
  const float expect_value[] = {10.f, 9.f, 8.f, 7.f, 6.f,
                                5.f,  4.f, 3.f, 2.f, 1.f};
  const float expect_avg[] = {0.55f, 0.5f,  0.45f, 0.4f,  0.35f,
                              0.3f,  0.25f, 0.2f,  0.15f, 0.1f};
  for (size_t delay = 0; delay < 10; ++delay) {
    const size_t num_items = 10 - delay;
    EXPECT_NEAR(*buffer_.ComputePartialMax(delay, num_items),
                expect_value[delay], 0.00001f);
    EXPECT_NEAR(*buffer_.ComputePartialAverage(delay, num_items),
                expect_avg[delay], 0.00001f);
    EXPECT_NEAR(*buffer_.ComputePartialMax(delay, 1), expect_value[delay],
                0.00001f);
    EXPECT_NEAR(*buffer_.ComputePartialAverage(delay, 1),
                expect_value[delay] / 10.f, 0.00001f);
  }
  const float expect_avg_short[] = {0.9f, 0.8f, 0.7f, 0.6f,
                                    0.5f, 0.4f, 0.3f, 0.2f};
  for (int delay = 0; delay < 8; ++delay) {
    EXPECT_NEAR(*buffer_.ComputePartialMax(delay, 3), expect_value[delay],
                0.00001f);
    EXPECT_NEAR(*buffer_.ComputePartialAverage(delay, 3),
                expect_avg_short[delay], 0.00001f);
  }
}

TEST_F(LevelBufferTest, RepeatedProcessingSuccessfull) {
  const size_t buffer_size = 2 * kBufferSize + 4;
  FillBuffer(buffer_size);

  EXPECT_EQ(buffer_.Size(), kBufferSize);
  EXPECT_EQ(buffer_.ComputePartialMax(0, 1), 24.0f);
  EXPECT_EQ(buffer_.ComputePartialMax(1, 4), 23.0f);
  EXPECT_EQ(buffer_.ComputePartialMax(2, 8), 22.0f);
  EXPECT_NEAR(*buffer_.ComputePartialAverage(0, 1), 2.4f, 0.000001f);
  EXPECT_NEAR(*buffer_.ComputePartialAverage(9, 1), 1.5f, 0.000001f);
  EXPECT_NEAR(*buffer_.ComputePartialAverage(2, 8), 1.85f, 0.000001f);

  EXPECT_NEAR(
      *buffer_.ComputePartialAverage(0, kNumFrames + kNumPreviousFrames),
      *buffer_.ComputeAverage(), 0.000001f);
  EXPECT_NEAR(
      *buffer_.ComputePartialAverage(0, kNumFrames + kNumPreviousFrames), 1.95f,
      0.000001f);
  EXPECT_NEAR(*buffer_.ComputePartialMax(0, kNumFrames + kNumPreviousFrames),
              *buffer_.ComputeMax(), 0.000001f);
  EXPECT_NEAR(*buffer_.ComputePartialMax(0, kNumFrames + kNumPreviousFrames),
              24.f, 0.000001f);

  EXPECT_EQ(buffer_.ComputePartialAverage(0, kNumFrames), 2.2f);
  EXPECT_EQ(buffer_.ComputePartialAverage(kNumFrames, kNumPreviousFrames),
            1.7f);
  EXPECT_EQ(buffer_.ComputePartialMax(0, kNumFrames), 24.f);
  EXPECT_EQ(buffer_.ComputePartialMax(kNumFrames, kNumPreviousFrames), 19.f);

  EXPECT_FALSE(buffer_.ComputePartialMax(kBufferSize, 1));
  EXPECT_FALSE(buffer_.ComputePartialMax(0, kBufferSize + 1));
  EXPECT_FALSE(buffer_.ComputePartialAverage(kBufferSize, 1));
  EXPECT_FALSE(buffer_.ComputePartialAverage(0, kBufferSize + 1));
}

TEST_F(LevelBufferTest, IncompleteBufferOverIndexingDetected) {
  const size_t num_items = 4;
  FillBuffer(num_items);
  for (size_t delay = 0; delay < num_items + 5; ++delay) {
    for (size_t length = 0; length < num_items + 5; ++length) {
      if (delay + length <= num_items && length > 0) {
        EXPECT_TRUE(buffer_.ComputePartialMax(delay, length));
      } else {
        EXPECT_FALSE(buffer_.ComputePartialMax(delay, length));
      }
    }
  }
}

TEST_F(LevelBufferTest, RepeatedProcessingOverIndexingDetected) {
  FillBuffer(2 * kBufferSize + 4);
  for (size_t delay = 0; delay < kBufferSize + 5; ++delay) {
    for (size_t length = 0; length < kBufferSize + 5; ++length) {
      if (delay + length <= kBufferSize && length > 0) {
        EXPECT_TRUE(buffer_.ComputePartialMax(delay, length));
      } else {
        EXPECT_FALSE(buffer_.ComputePartialMax(delay, length));
      }
    }
  }
}

}  // namespace webrtc
