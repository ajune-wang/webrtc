/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_digital_gain_applier.h"

#include <algorithm>

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/vector_float_frame.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/audio_processing/vad/vad_with_level.h"

// #include "modules/audio_processing/agc2/fixed_digital_level_estimator.h"

// #include <iostream>

// #include "modules/audio_processing/agc2/agc2_testing_common.h"
// #include "modules/audio_processing/agc2/vector_float_frame.h"
#include "rtc_base/gunit.h"
// #include "test/gtest.h"

namespace webrtc {
namespace {
constexpr float kNoNoiseDbfs = -90.f;
constexpr float kHighNoiseDbfs = -20.f;

// Runs gain applier and returns the applied gain in linear scale.
float RunOnConstantLevel(int num_iterations,
                         VadWithLevel::LevelAndProbability vad_data,
                         float input_level_dbfs,
                         AdaptiveDigitalGainApplier* gain_applier) {
  float gain_linear = 0.f;

  for (int i = 0; i < num_iterations; ++i) {
    VectorFloatFrame fake_audio(1, 1, 1.f);
    gain_applier->Process(
        input_level_dbfs, kNoNoiseDbfs,
        rtc::ArrayView<const VadWithLevel::LevelAndProbability>(&vad_data, 1),
        fake_audio.float_frame_view());
    gain_linear = fake_audio.float_frame_view().channel(0)[0];
  }
  return gain_linear;
}

constexpr VadWithLevel::LevelAndProbability kVadSpeech(1.f, -20.f, 0.f);
}  // namespace

TEST(AutomaticGainController2AdaptiveGainApplier, GainApplierShouldNotCrash) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  // Make one call with reasonable audio level values and settings.
  VectorFloatFrame fake_audio(2, 480, 10000.f);
  gain_applier.Process(
      -5.0, kNoNoiseDbfs,
      rtc::ArrayView<const VadWithLevel::LevelAndProbability>(&kVadSpeech, 1),
      fake_audio.float_frame_view());
}

// Check that the output is -kHeadroom dBFS
TEST(AutomaticGainController2AdaptiveGainApplier, TargetLevelIsReached) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  constexpr float kInitialLevel = -5.f;

  const float applied_gain =
      RunOnConstantLevel(200, kVadSpeech, kInitialLevel, &gain_applier);

  EXPECT_NEAR(applied_gain, DbToRatio(-kHeadroomDbfs - kInitialLevel), 0.1f);
}

// Check that the output is -kHeadroom dBFS
TEST(AutomaticGainController2AdaptiveGainApplier, GainApproachesMaxGain) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  constexpr float kInitialLevel = -kHeadroomDbfs - kMaxGainDb - 10.f;
  // A few extra frames for safety.
  constexpr int kNumFramesToAdapt =
      static_cast<int>(kMaxGainDb / kMaxGainChangePerFrameDb) + 10;

  const float applied_gain = RunOnConstantLevel(kNumFramesToAdapt, kVadSpeech,
                                                kInitialLevel, &gain_applier);
  EXPECT_NEAR(applied_gain, DbToRatio(kMaxGainDb), 0.1f);

  const float applied_gain_db = 20.f * std::log10(applied_gain);
  EXPECT_NEAR(applied_gain_db, kMaxGainDb, 0.1f);
}

TEST(AutomaticGainController2AdaptiveGainApplier, GainDoesNotChangeFast) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  constexpr float kInitialLevel = -25.f;
  // A few extra frames for safety.
  constexpr int kNumFramesToAdapt =
      static_cast<int>(kInitialLevel / kMaxGainChangePerFrameDb) + 10;

  const float kMaxChangePerFrameLinear = DbToRatio(kMaxGainChangePerFrameDb);

  float last_gain_linear = 1.f;
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    VectorFloatFrame fake_audio(1, 1, 1.f);
    gain_applier.Process(
        kInitialLevel, kNoNoiseDbfs,
        rtc::ArrayView<const VadWithLevel::LevelAndProbability>(&kVadSpeech, 1),
        fake_audio.float_frame_view());
    float current_gain_linear = fake_audio.float_frame_view().channel(0)[0];
    EXPECT_LE(std::abs(current_gain_linear - last_gain_linear),
              kMaxChangePerFrameLinear);
    last_gain_linear = current_gain_linear;
  }

  // Check that the same is true when gain decreases as well.
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    VectorFloatFrame fake_audio(1, 1, 1.f);
    gain_applier.Process(
        0.f, kNoNoiseDbfs,
        rtc::ArrayView<const VadWithLevel::LevelAndProbability>(&kVadSpeech, 1),
        fake_audio.float_frame_view());
    float current_gain_linear = fake_audio.float_frame_view().channel(0)[0];
    EXPECT_LE(std::abs(current_gain_linear - last_gain_linear),
              kMaxChangePerFrameLinear * 1.01f);
    last_gain_linear = current_gain_linear;
  }
}

TEST(AutomaticGainController2AdaptiveGainApplier, GainIsRampedInAFrame) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  constexpr float kInitialLevel = -25.f;
  constexpr int kNumSamples = 480;

  VectorFloatFrame fake_audio(1, kNumSamples, 1.f);
  gain_applier.Process(
      kInitialLevel, kNoNoiseDbfs,
      rtc::ArrayView<const VadWithLevel::LevelAndProbability>(&kVadSpeech, 1),
      fake_audio.float_frame_view());
  float maximal_difference = 0.f;
  float current_value = 1.f;
  for (const auto& x : fake_audio.float_frame_view().channel(0)) {
    const float difference = std::abs(x - current_value);
    maximal_difference = std::max(maximal_difference, difference);
    current_value = x;
  }

  const float kMaxChangePerFrameLinear = DbToRatio(kMaxGainChangePerFrameDb);
  const float kMaxChangePerSample = kMaxChangePerFrameLinear / kNumSamples;

  EXPECT_LE(maximal_difference, kMaxChangePerSample * 1.01f);
}

TEST(AutomaticGainController2AdaptiveGainApplier, NoiseLimitsGain) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  constexpr float kInitialLevel = -25.f;
  constexpr int kNumSamples = 480;
  constexpr int kNumFrames = 100;

  for (int i = 0; i < kNumFrames; ++i) {
    VectorFloatFrame fake_audio(1, kNumSamples, 1.f);
    gain_applier.Process(
        kInitialLevel, kHighNoiseDbfs,
        rtc::ArrayView<const VadWithLevel::LevelAndProbability>(&kVadSpeech, 1),
        fake_audio.float_frame_view());

    const float maximal_ratio =
        *std::max_element(fake_audio.float_frame_view().channel(0).begin(),
                          fake_audio.float_frame_view().channel(0).end());

    EXPECT_NEAR(maximal_ratio, 1.f, 0.001f);
  }
}
}  // namespace webrtc
