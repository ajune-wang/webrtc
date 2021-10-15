/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/analog_gain_stats_reporter.h"

#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(AnalogGainStatsReporterTest, CheckLevelUpdateStatsForEmptyStats) {
  AnalogGainStatsReporter stats_reporter;
  const auto& update_stats = stats_reporter.level_update_stats();
  EXPECT_EQ(update_stats.num_decreases, 0);
  EXPECT_EQ(update_stats.sum_decreases, 0);
  EXPECT_EQ(update_stats.num_increases, 0);
  EXPECT_EQ(update_stats.sum_increases, 0);
}

TEST(AnalogGainStatsReporterTest, CheckLevelUpdateStatsAfterNoGainChange) {
  constexpr int kMicLevel = 10;
  AnalogGainStatsReporter stats_reporter;
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel);
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel);
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel);
  const auto& update_stats = stats_reporter.level_update_stats();
  EXPECT_EQ(update_stats.num_decreases, 0);
  EXPECT_EQ(update_stats.sum_decreases, 0);
  EXPECT_EQ(update_stats.num_increases, 0);
  EXPECT_EQ(update_stats.sum_increases, 0);
}

TEST(AnalogGainStatsReporterTest, CheckLevelUpdateStatsAfterGainIncrease) {
  constexpr int kMicLevel = 10;
  AnalogGainStatsReporter stats_reporter;
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel);
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel + 4);
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel + 5);
  const auto& update_stats = stats_reporter.level_update_stats();
  EXPECT_EQ(update_stats.num_decreases, 0);
  EXPECT_EQ(update_stats.sum_decreases, 0);
  EXPECT_EQ(update_stats.num_increases, 2);
  EXPECT_EQ(update_stats.sum_increases, 5);
}

TEST(AnalogGainStatsReporterTest, CheckLevelUpdateStatsAfterGainDecrease) {
  constexpr int kMicLevel = 10;
  AnalogGainStatsReporter stats_reporter;
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel);
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel - 4);
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel - 5);
  const auto& stats_update = stats_reporter.level_update_stats();
  EXPECT_EQ(stats_update.num_decreases, 2);
  EXPECT_EQ(stats_update.sum_decreases, 5);
  EXPECT_EQ(stats_update.num_increases, 0);
  EXPECT_EQ(stats_update.sum_increases, 0);
}

TEST(AnalogGainStatsReporterTest, CheckLevelUpdateStatsAfterReset) {
  AnalogGainStatsReporter stats_reporter;
  constexpr int kMicLevel = 10;
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel);
  // Update until the periodic reset.
  constexpr int kFramesIn60Seconds = 6000;
  for (int i = 0; i < kFramesIn60Seconds - 2; i += 2) {
    stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel + 2);
    stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel);
  }
  const auto& stats_before_reset = stats_reporter.level_update_stats();
  EXPECT_EQ(stats_before_reset.num_decreases, kFramesIn60Seconds / 2 - 1);
  EXPECT_EQ(stats_before_reset.sum_decreases, kFramesIn60Seconds - 2);
  EXPECT_EQ(stats_before_reset.num_increases, kFramesIn60Seconds / 2 - 1);
  EXPECT_EQ(stats_before_reset.sum_increases, kFramesIn60Seconds - 2);
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel + 2);
  const auto& stats_during_reset = stats_reporter.level_update_stats();
  EXPECT_EQ(stats_during_reset.num_decreases, 0);
  EXPECT_EQ(stats_during_reset.sum_decreases, 0);
  EXPECT_EQ(stats_during_reset.num_increases, 0);
  EXPECT_EQ(stats_during_reset.sum_increases, 0);
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel);
  stats_reporter.UpdateStatistics(/*analog_mic_level=*/kMicLevel + 3);
  const auto& stats_after_reset = stats_reporter.level_update_stats();
  EXPECT_EQ(stats_after_reset.num_decreases, 1);
  EXPECT_EQ(stats_after_reset.sum_decreases, 2);
  EXPECT_EQ(stats_after_reset.num_increases, 1);
  EXPECT_EQ(stats_after_reset.sum_increases, 3);
}

}  // namespace
}  // namespace webrtc
