
/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "video/quality_convergence_monitor.h"

#include <vector>

#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {
namespace {
constexpr int kStaticQpThreshold = 13;
constexpr QualityConvergenceMonitor::Parameters kParametersOnlyStaticThreshold =
    {.static_qp_threshold = kStaticQpThreshold,
     .dynamic_detection_enabled = false};
constexpr QualityConvergenceMonitor::Parameters
    kParametersWithDynamicDetection = {
        .static_qp_threshold = kStaticQpThreshold,
        .dynamic_detection_enabled = true,
        .window_length = 12,
        .tail_length = 3,
        .dynamic_qp_threshold = 24};

// Test the basics of the algorithm.

TEST(QualityConvergenceMonitorAlgorithm, StaticThreshold) {
  QualityConvergenceMonitor::Parameters p = kParametersOnlyStaticThreshold;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);
  ASSERT_TRUE(monitor);

  std::vector<bool> steady_states = {false, true};
  for (bool steady_state_fresh_frame : steady_states) {
    // Ramp down from 100. Not at target quality until qp <= static threshold.
    int qp = 100;
    while (qp > p.static_qp_threshold) {
      monitor->AddSample(qp, steady_state_fresh_frame);
      EXPECT_FALSE(monitor->AtTargetQuality());
      qp -= 1;
    }

    monitor->AddSample(qp, steady_state_fresh_frame);
    EXPECT_TRUE(monitor->AtTargetQuality());

    // 100 samples just above the threshold is not at target quality.
    for (int i = 0; i < 100; ++i) {
      monitor->AddSample(p.static_qp_threshold + 1, steady_state_fresh_frame);
      EXPECT_FALSE(monitor->AtTargetQuality());
    }
  }
}

TEST(QualityConvergenceMonitorAlgorithm,
     StaticThresholdWithDynamicDetectionEnabled) {
  QualityConvergenceMonitor::Parameters p = kParametersWithDynamicDetection;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);
  ASSERT_TRUE(monitor);

  std::vector<bool> steady_states = {false, true};
  for (bool steady_state_fresh_frame : steady_states) {
    int qp = 100;
    // Clear buffer.
    monitor->AddSample(qp, /*is_steady_state_refresh_frame=*/false);
    EXPECT_FALSE(monitor->AtTargetQuality());

    // Ramp down from 100. Not at target quality until qp <= static threshold.
    while (qp > p.static_qp_threshold) {
      monitor->AddSample(qp, steady_state_fresh_frame);
      EXPECT_FALSE(monitor->AtTargetQuality());
      qp -= 1;
    }

    monitor->AddSample(qp, steady_state_fresh_frame);
    EXPECT_TRUE(monitor->AtTargetQuality());
  }

  // 100 samples just above the threshold is not at target quality if it's not a
  // steady state frame.
  for (int i = 0; i < 100; ++i) {
    monitor->AddSample(p.static_qp_threshold + 1,
                       /*is_steady_state_refresh_frame=*/false);
    EXPECT_FALSE(monitor->AtTargetQuality());
  }
}

TEST(QualityConvergenceMonitorAlgorithm, ConvergenceAtDynamicThreshold) {
  QualityConvergenceMonitor::Parameters p = kParametersWithDynamicDetection;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);
  ASSERT_TRUE(monitor);

  // `window_length` steady-state frames at the dynamic threshold must mean
  // we're at target quality.
  for (size_t i = 0; i < p.window_length; ++i) {
    monitor->AddSample(p.dynamic_qp_threshold,
                       /*is_steady_state_refresh_frame=*/true);
  }
  EXPECT_TRUE(monitor->AtTargetQuality());
}

TEST(QualityConvergenceMonitorAlgorithm, NoConvergenceAboveDynamicThreshold) {
  QualityConvergenceMonitor::Parameters p = kParametersWithDynamicDetection;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);
  ASSERT_TRUE(monitor);

  // 100 samples just above the threshold must imply that we're not at target
  // quality.
  for (int i = 0; i < 100; ++i) {
    monitor->AddSample(p.dynamic_qp_threshold + 1,
                       /*is_steady_state_refresh_frame=*/true);
    EXPECT_FALSE(monitor->AtTargetQuality());
  }
}

TEST(QualityConvergenceMonitorAlgorithm,
     MaintainAtTargetQualityForSteadyStateFrames) {
  QualityConvergenceMonitor::Parameters p = kParametersWithDynamicDetection;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);
  ASSERT_TRUE(monitor);

  // `window_length` steady-state frames at the dynamic threshold must mean
  // we're at target quality.
  for (size_t i = 0; i < p.window_length; ++i) {
    monitor->AddSample(p.dynamic_qp_threshold,
                       /*is_steady_state_refresh_frame=*/true);
  }
  EXPECT_TRUE(monitor->AtTargetQuality());

  int qp = p.dynamic_qp_threshold;
  for (int i = 0; i < 100; ++i) {
    monitor->AddSample(qp++, /*is_steady_state_refresh_frame=*/true);
    EXPECT_TRUE(monitor->AtTargetQuality());
  }

  // Reset state for first frame that is not steady state.
  monitor->AddSample(qp, /*is_steady_state_refresh_frame=*/false);
  EXPECT_FALSE(monitor->AtTargetQuality());
}

// Test corner cases.

TEST(QualityConvergenceMonitorAlgorithm, SufficientData) {
  QualityConvergenceMonitor::Parameters p = kParametersWithDynamicDetection;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);
  ASSERT_TRUE(monitor);

  // Less than `tail_length + 1` steady-state QP values at the dynamic threshold
  // is not sufficient.
  for (size_t i = 0; i < p.tail_length; ++i) {
    monitor->AddSample(p.dynamic_qp_threshold,
                       /*is_steady_state_refresh_frame=*/true);
    // Not sufficient data
    EXPECT_FALSE(monitor->AtTargetQuality());
  }

  // However, `tail_length + 1` QP values are sufficient.
  monitor->AddSample(p.dynamic_qp_threshold,
                     /*is_steady_state_refresh_frame=*/true);
  EXPECT_TRUE(monitor->AtTargetQuality());
}

TEST(QualityConvergenceMonitorAlgorithm,
     AtTargetIfQpHeadLessThanOrEqualToQpTail) {
  QualityConvergenceMonitor::Parameters p = kParametersWithDynamicDetection;
  p.window_length = 6;
  p.tail_length = 3;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);

  // Sequence for which QP_head > QP_tail.
  std::vector<int> head_gt_tail_qps = {23, 21, 21, 21, 21, 22};
  for (int qp : head_gt_tail_qps) {
    monitor->AddSample(qp,
                       /*is_steady_state_refresh_frame=*/true);
    EXPECT_FALSE(monitor->AtTargetQuality());
  }

  // Reset QP window.
  monitor->AddSample(-1,
                     /*is_steady_state_refresh_frame=*/false);
  EXPECT_FALSE(monitor->AtTargetQuality());

  // Sequence for which the last sample will make QP_head == QP_tail.
  std::vector<int> head_equal_tail_qps = {22, 21, 21, 21, 21, 22};
  for (unsigned int i = 0; i < head_equal_tail_qps.size() - 1; ++i) {
    monitor->AddSample(head_equal_tail_qps[i],
                       /*is_steady_state_refresh_frame=*/true);
    EXPECT_FALSE(monitor->AtTargetQuality());
  }
  monitor->AddSample(head_equal_tail_qps.back(),
                     /*is_steady_state_refresh_frame=*/true);
  EXPECT_TRUE(monitor->AtTargetQuality());

  // Reset QP window.
  monitor->AddSample(-1,
                     /*is_steady_state_refresh_frame=*/false);
  EXPECT_FALSE(monitor->AtTargetQuality());

  // Sequence for which the last sample will make QP_head < QP_tail.
  std::vector<int> head_lt_tail_qps = {22, 21, 21, 21, 21, 23};
  for (unsigned int i = 0; i < head_lt_tail_qps.size() - 1; ++i) {
    monitor->AddSample(head_lt_tail_qps[i],
                       /*is_steady_state_refresh_frame=*/true);
    EXPECT_FALSE(monitor->AtTargetQuality());
  }
  monitor->AddSample(head_lt_tail_qps.back(),
                     /*is_steady_state_refresh_frame=*/true);
  EXPECT_TRUE(monitor->AtTargetQuality());
}

// Test default values and that they can be overridden with field trials.

TEST(QualityConvergenceMonitorSetup, DefaultParameters) {
  test::ScopedKeyValueConfig field_trials;
  auto monitor = QualityConvergenceMonitor::Create(
      kStaticQpThreshold, kVideoCodecVP8, field_trials);
  ASSERT_TRUE(monitor);
  QualityConvergenceMonitor::Parameters vp8_parameters =
      monitor->GetParametersForTesting();
  EXPECT_EQ(vp8_parameters.static_qp_threshold, kStaticQpThreshold);
  EXPECT_FALSE(vp8_parameters.dynamic_detection_enabled);

  monitor = QualityConvergenceMonitor::Create(kStaticQpThreshold,
                                              kVideoCodecVP9, field_trials);
  ASSERT_TRUE(monitor);
  QualityConvergenceMonitor::Parameters vp9_parameters =
      monitor->GetParametersForTesting();
  EXPECT_EQ(vp9_parameters.static_qp_threshold, kStaticQpThreshold);
  EXPECT_TRUE(vp9_parameters.dynamic_detection_enabled);
  EXPECT_EQ(vp9_parameters.dynamic_qp_threshold, 28);  // 13 + 15.
  EXPECT_EQ(vp9_parameters.window_length, 12u);
  EXPECT_EQ(vp9_parameters.tail_length, 6u);

  monitor = QualityConvergenceMonitor::Create(kStaticQpThreshold,
                                              kVideoCodecAV1, field_trials);
  ASSERT_TRUE(monitor);
  QualityConvergenceMonitor::Parameters av1_parameters =
      monitor->GetParametersForTesting();
  EXPECT_EQ(av1_parameters.static_qp_threshold, kStaticQpThreshold);
  EXPECT_TRUE(av1_parameters.dynamic_detection_enabled);
  EXPECT_EQ(av1_parameters.dynamic_qp_threshold, 28);  // 13 + 15.
  EXPECT_EQ(av1_parameters.window_length, 12u);
  EXPECT_EQ(av1_parameters.tail_length, 6u);
}

TEST(QualityConvergenceMonitorSetup, OverrideVp8Parameters) {
  test::ScopedKeyValueConfig field_trials(
      "WebRTC-QCM-Dynamic-VP8/"
      "enabled:1,alpha:0.08,window_length:10,tail_length:4/");

  auto monitor = QualityConvergenceMonitor::Create(
      kStaticQpThreshold, kVideoCodecVP8, field_trials);
  ASSERT_TRUE(monitor);
  QualityConvergenceMonitor::Parameters p = monitor->GetParametersForTesting();
  EXPECT_EQ(p.static_qp_threshold, kStaticQpThreshold);
  EXPECT_TRUE(p.dynamic_detection_enabled);
  EXPECT_EQ(p.dynamic_qp_threshold, 23);  // 13 + 10.
  EXPECT_EQ(p.window_length, 10u);
  EXPECT_EQ(p.tail_length, 4u);
}

TEST(QualityConvergenceMonitorSetup, OverrideVp9Parameters) {
  test::ScopedKeyValueConfig field_trials(
      "WebRTC-QCM-Dynamic-VP9/"
      "enabled:1,alpha:0.08,window_length:10,tail_length:4/");

  auto monitor = QualityConvergenceMonitor::Create(
      kStaticQpThreshold, kVideoCodecVP9, field_trials);
  ASSERT_TRUE(monitor);
  QualityConvergenceMonitor::Parameters p = monitor->GetParametersForTesting();
  EXPECT_EQ(p.static_qp_threshold, kStaticQpThreshold);
  EXPECT_TRUE(p.dynamic_detection_enabled);
  EXPECT_EQ(p.dynamic_qp_threshold, 33);  // 13 + 20.
  EXPECT_EQ(p.window_length, 10u);
  EXPECT_EQ(p.tail_length, 4u);
}

TEST(QualityConvergenceMonitorSetup, OverrideAv1Parameters) {
  test::ScopedKeyValueConfig field_trials(
      "WebRTC-QCM-Dynamic-AV1/"
      "enabled:1,alpha:0.10,window_length:16,tail_length:8/");

  auto monitor = QualityConvergenceMonitor::Create(
      kStaticQpThreshold, kVideoCodecAV1, field_trials);
  ASSERT_TRUE(monitor);
  QualityConvergenceMonitor::Parameters p = monitor->GetParametersForTesting();
  EXPECT_EQ(p.static_qp_threshold, kStaticQpThreshold);
  EXPECT_TRUE(p.dynamic_detection_enabled);
  EXPECT_EQ(p.dynamic_qp_threshold, 38);  // 13 + 25.
  EXPECT_EQ(p.window_length, 16u);
  EXPECT_EQ(p.tail_length, 8u);
}

TEST(QualityConvergenceMonitorSetup, DisableVp9Dynamic) {
  test::ScopedKeyValueConfig field_trials("WebRTC-QCM-Dynamic-VP9/enabled:0/");

  auto monitor = QualityConvergenceMonitor::Create(
      kStaticQpThreshold, kVideoCodecVP9, field_trials);
  ASSERT_TRUE(monitor);
  QualityConvergenceMonitor::Parameters p = monitor->GetParametersForTesting();
  EXPECT_FALSE(p.dynamic_detection_enabled);
}

TEST(QualityConvergenceMonitorSetup, DisableAv1Dynamic) {
  test::ScopedKeyValueConfig field_trials("WebRTC-QCM-Dynamic-AV1/enabled:0/");

  auto monitor = QualityConvergenceMonitor::Create(
      kStaticQpThreshold, kVideoCodecAV1, field_trials);
  ASSERT_TRUE(monitor);
  QualityConvergenceMonitor::Parameters p = monitor->GetParametersForTesting();
  EXPECT_FALSE(p.dynamic_detection_enabled);
}

}  // namespace
}  // namespace webrtc
