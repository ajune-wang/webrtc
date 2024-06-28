
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

namespace webrtc {
namespace {

constexpr QualityConvergenceMonitor::Parameters kParametersOnlyStaticThreshold =
    {.static_qp_threshold = 13, .dynamic_detection_enabled = false};
constexpr QualityConvergenceMonitor::Parameters
    kParametersWithDynamicDetection = {.static_qp_threshold = 13,
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

}  // namespace
}  // namespace webrtc
