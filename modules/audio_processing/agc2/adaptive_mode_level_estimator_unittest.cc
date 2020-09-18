/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_mode_level_estimator.h"

#include <memory>

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

constexpr float kSaturationProtectorInitialMarginDb = 20.f;
constexpr float kSaturationProtectorExtraMarginDb = 2.f;

static_assert(kInitialSpeechLevelEstimateDbfs < 0.f, "");
constexpr float kVadLevelRms = kInitialSpeechLevelEstimateDbfs / 2.f;
constexpr float kVadLevelPeak = kInitialSpeechLevelEstimateDbfs / 3.f;

constexpr VadWithLevel::LevelAndProbability kVadDataSpeech(
    /*prob=*/1.f,
    kVadLevelRms,
    kVadLevelPeak);
constexpr VadWithLevel::LevelAndProbability kVadDataNonSpeech(
    /*prob=*/kVadConfidenceThreshold / 2.f,
    kVadLevelRms,
    kVadLevelPeak);

void RunOnConstantLevel(int num_iterations,
                        const VadWithLevel::LevelAndProbability& vad_data,
                        AdaptiveModeLevelEstimator& level_estimator) {
  for (int i = 0; i < num_iterations; ++i) {
    level_estimator.Update(vad_data);
  }
}

std::unique_ptr<AdaptiveModeLevelEstimator> CreateAdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper) {
  return std::make_unique<AdaptiveModeLevelEstimator>(
      apm_data_dumper,
      AudioProcessing::Config::GainController2::LevelEstimator::kRms,
      /*min_consecutive_speech_frames=*/1,
      /*use_saturation_protector=*/true, kSaturationProtectorInitialMarginDb,
      kSaturationProtectorExtraMarginDb);
}

TEST(AutomaticGainController2AdaptiveModeLevelEstimator,
     EstimatorShouldNotCrash) {
  ApmDataDumper apm_data_dumper(0);
  auto level_estimator = CreateAdaptiveModeLevelEstimator(&apm_data_dumper);

  VadWithLevel::LevelAndProbability vad_data(1.f, -20.f, -10.f);
  level_estimator->Update(vad_data);
  static_cast<void>(level_estimator->GetLevelDbfs());
}

TEST(AutomaticGainController2AdaptiveModeLevelEstimator, LevelShouldStabilize) {
  ApmDataDumper apm_data_dumper(0);
  auto level_estimator = CreateAdaptiveModeLevelEstimator(&apm_data_dumper);

  constexpr float kSpeechPeakDbfs = -15.f;
  RunOnConstantLevel(
      100,
      VadWithLevel::LevelAndProbability(
          1.f, kSpeechPeakDbfs - kSaturationProtectorInitialMarginDb,
          kSpeechPeakDbfs),
      *level_estimator);

  EXPECT_NEAR(
      level_estimator->GetLevelDbfs() - kSaturationProtectorExtraMarginDb,
      kSpeechPeakDbfs, 0.1f);
}

TEST(AutomaticGainController2AdaptiveModeLevelEstimator,
     EstimatorIgnoresZeroProbabilityFrames) {
  ApmDataDumper apm_data_dumper(0);
  auto level_estimator = CreateAdaptiveModeLevelEstimator(&apm_data_dumper);

  // Run for one second of fake audio.
  constexpr float kSpeechRmsDbfs = -25.f;
  RunOnConstantLevel(
      100,
      VadWithLevel::LevelAndProbability(
          1.f, kSpeechRmsDbfs - kSaturationProtectorInitialMarginDb,
          kSpeechRmsDbfs),
      *level_estimator);

  // Run for one more second, but mark as not speech.
  constexpr float kNoiseRmsDbfs = 0.f;
  RunOnConstantLevel(
      100, VadWithLevel::LevelAndProbability(0.f, kNoiseRmsDbfs, kNoiseRmsDbfs),
      *level_estimator);

  // Level should not have changed.
  EXPECT_NEAR(
      level_estimator->GetLevelDbfs() - kSaturationProtectorExtraMarginDb,
      kSpeechRmsDbfs, 0.1f);
}

TEST(AutomaticGainController2AdaptiveModeLevelEstimator, TimeToAdapt) {
  ApmDataDumper apm_data_dumper(0);
  auto level_estimator = CreateAdaptiveModeLevelEstimator(&apm_data_dumper);

  // Run for one 'window size' interval.
  constexpr float kInitialSpeechRmsDbfs = -30.f;
  RunOnConstantLevel(
      kFullBufferSizeMs / kFrameDurationMs,
      VadWithLevel::LevelAndProbability(
          1.f, kInitialSpeechRmsDbfs - kSaturationProtectorInitialMarginDb,
          kInitialSpeechRmsDbfs),
      *level_estimator);

  // Run for one half 'window size' interval. This should not be enough to
  // adapt.
  constexpr float kDifferentSpeechRmsDbfs = -10.f;
  // It should at most differ by 25% after one half 'window size' interval.
  const float kMaxDifferenceDb =
      0.25 * std::abs(kDifferentSpeechRmsDbfs - kInitialSpeechRmsDbfs);
  RunOnConstantLevel(
      static_cast<int>(kFullBufferSizeMs / kFrameDurationMs / 2),
      VadWithLevel::LevelAndProbability(
          1.f, kDifferentSpeechRmsDbfs - kSaturationProtectorInitialMarginDb,
          kDifferentSpeechRmsDbfs),
      *level_estimator);
  EXPECT_GT(std::abs(kDifferentSpeechRmsDbfs - level_estimator->GetLevelDbfs()),
            kMaxDifferenceDb);

  // Run for some more time. Afterwards, we should have adapted.
  RunOnConstantLevel(
      static_cast<int>(3 * kFullBufferSizeMs / kFrameDurationMs),
      VadWithLevel::LevelAndProbability(
          1.f, kDifferentSpeechRmsDbfs - kSaturationProtectorInitialMarginDb,
          kDifferentSpeechRmsDbfs),
      *level_estimator);
  EXPECT_NEAR(
      level_estimator->GetLevelDbfs() - kSaturationProtectorExtraMarginDb,
      kDifferentSpeechRmsDbfs, kMaxDifferenceDb * 0.5f);
}

TEST(AutomaticGainController2AdaptiveModeLevelEstimator,
     ResetGivesFastAdaptation) {
  ApmDataDumper apm_data_dumper(0);
  auto level_estimator = CreateAdaptiveModeLevelEstimator(&apm_data_dumper);

  // Run the level estimator for one window size interval. This gives time to
  // adapt.
  constexpr float kInitialSpeechRmsDbfs = -30.f;
  RunOnConstantLevel(
      kFullBufferSizeMs / kFrameDurationMs,
      VadWithLevel::LevelAndProbability(
          1.f, kInitialSpeechRmsDbfs - kSaturationProtectorInitialMarginDb,
          kInitialSpeechRmsDbfs),
      *level_estimator);

  constexpr float kDifferentSpeechRmsDbfs = -10.f;
  // Reset and run one half window size interval.
  level_estimator->Reset();

  RunOnConstantLevel(
      kFullBufferSizeMs / kFrameDurationMs / 2,
      VadWithLevel::LevelAndProbability(
          1.f, kDifferentSpeechRmsDbfs - kSaturationProtectorInitialMarginDb,
          kDifferentSpeechRmsDbfs),
      *level_estimator);

  // The level should be close to 'kDifferentSpeechRmsDbfs'.
  const float kMaxDifferenceDb =
      0.1f * std::abs(kDifferentSpeechRmsDbfs - kInitialSpeechRmsDbfs);
  EXPECT_LT(
      std::abs(kDifferentSpeechRmsDbfs - (level_estimator->GetLevelDbfs() -
                                          kSaturationProtectorExtraMarginDb)),
      kMaxDifferenceDb);
}

struct TestConfig {
  int min_consecutive_speech_frames;
  bool use_saturation_protector;
  float initial_saturation_margin_db;
  float extra_saturation_margin_db;
};

class AdaptiveModeLevelEstimatorTest
    : public ::testing::TestWithParam<TestConfig> {};

TEST_P(AdaptiveModeLevelEstimatorTest, DoNotAdaptToShortSpeechSegments) {
  const auto params = GetParam();
  ApmDataDumper apm_data_dumper(0);
  AdaptiveModeLevelEstimator level_estimator(
      &apm_data_dumper,
      AudioProcessing::Config::GainController2::LevelEstimator::kRms,
      params.min_consecutive_speech_frames, params.use_saturation_protector,
      params.initial_saturation_margin_db, params.extra_saturation_margin_db);
  const float initial_level = level_estimator.GetLevelDbfs();
  ASSERT_LT(initial_level, kVadDataSpeech.speech_rms_dbfs);
  for (int i = 0; i < params.min_consecutive_speech_frames - 1; ++i) {
    SCOPED_TRACE(i);
    level_estimator.Update(kVadDataSpeech);
    EXPECT_EQ(initial_level, level_estimator.GetLevelDbfs());
  }
  level_estimator.Update(kVadDataNonSpeech);
  EXPECT_EQ(initial_level, level_estimator.GetLevelDbfs());
}

TEST_P(AdaptiveModeLevelEstimatorTest, AdaptToEnoughSpeechSegments) {
  const auto params = GetParam();
  ApmDataDumper apm_data_dumper(0);
  AdaptiveModeLevelEstimator level_estimator(
      &apm_data_dumper,
      AudioProcessing::Config::GainController2::LevelEstimator::kRms,
      params.min_consecutive_speech_frames, params.use_saturation_protector,
      params.initial_saturation_margin_db, params.extra_saturation_margin_db);
  const float initial_level = level_estimator.GetLevelDbfs();
  ASSERT_LT(initial_level, kVadDataSpeech.speech_rms_dbfs);
  for (int i = 0; i < params.min_consecutive_speech_frames; ++i) {
    level_estimator.Update(kVadDataSpeech);
  }
  EXPECT_LT(initial_level, level_estimator.GetLevelDbfs());
}

INSTANTIATE_TEST_SUITE_P(AutomaticGainController2,
                         AdaptiveModeLevelEstimatorTest,
                         ::testing::Values(TestConfig{1, false, 0.f, 0.f},
                                           TestConfig{1, true, 0.f, 0.f},
                                           TestConfig{9, false, 0.f, 0.f},
                                           TestConfig{9, true, 0.f, 0.f}));

}  // namespace
}  // namespace webrtc
