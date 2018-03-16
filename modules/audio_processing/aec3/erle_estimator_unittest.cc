/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/erle_estimator.h"
#include "test/gtest.h"

namespace webrtc {

namespace erle_unittest {

constexpr int kLowFrequencyLimit = kFftLengthBy2 / 2;
constexpr float max_erle_lf = 8.f;
constexpr float max_erle_hf = 1.5f;
constexpr float min_erle = 1.0f;
constexpr float trueErle = 10.f;
constexpr float trueErleOnsets = 1.0f;

static void VerifyErleBands(const std::array<float, kFftLengthBy2Plus1>& erle,
                            float reference_lf,
                            float reference_hf) {
  std::for_each(
      erle.begin(), erle.begin() + kLowFrequencyLimit,
      [reference_lf](float a) { EXPECT_NEAR(reference_lf, a, 0.001); });
  std::for_each(
      erle.begin() + kLowFrequencyLimit, erle.end(),
      [reference_hf](float a) { EXPECT_NEAR(reference_hf, a, 0.001); });
}

static void VerifyErle(const std::array<float, kFftLengthBy2Plus1>& erle,
                       float erle_time_domain,
                       float reference_lf,
                       float reference_hf) {
  VerifyErleBands(erle, reference_lf, reference_hf);
  EXPECT_NEAR(reference_lf, erle_time_domain, 0.001);
}

static void farendFrame(std::array<float, kFftLengthBy2Plus1>& X2,
                        std::array<float, kFftLengthBy2Plus1>& E2,
                        std::array<float, kFftLengthBy2Plus1>& Y2,
                        float erle) {
  X2.fill(500 * 1000.f * 1000.f);
  E2.fill(1000.f * 1000.f);
  Y2.fill(erle * E2[0]);
}

static void nearendFrame(std::array<float, kFftLengthBy2Plus1>& X2,
                         std::array<float, kFftLengthBy2Plus1>& E2,
                         std::array<float, kFftLengthBy2Plus1>& Y2) {
  X2.fill(0.f);
  Y2.fill(500.f * 1000.f * 1000.f);
  E2.fill(Y2[0]);
}

static void VerifyVaryingEnvironment(std::array<float, kFftLengthBy2Plus1>& X2,
                                     std::array<float, kFftLengthBy2Plus1>& E2,
                                     std::array<float, kFftLengthBy2Plus1>& Y2,
                                     ErleEstimator& estimator) {
  for (size_t burst = 0; burst < 20; ++burst) {
    farendFrame(X2, E2, Y2, trueErleOnsets);
    for (size_t k = 0; k < 10; ++k) {
      estimator.Update(X2, Y2, E2);
    }
    farendFrame(X2, E2, Y2, trueErle);
    for (size_t k = 0; k < 200; ++k) {
      estimator.Update(X2, Y2, E2);
    }
    nearendFrame(X2, E2, Y2);
    for (size_t k = 0; k < 100; ++k) {
      estimator.Update(X2, Y2, E2);
    }
  }
  VerifyErleBands(estimator.ErleOnsets(), min_erle, min_erle);
  nearendFrame(X2, E2, Y2);
  for (size_t k = 0; k < 1000; k++) {
    estimator.Update(X2, Y2, E2);
  }
  // Verifies that during ne activity, Erle converges to the Erle for onsets
  VerifyErle(estimator.Erle(), estimator.ErleTimeDomain(), min_erle, min_erle);
}

static void VerifyIncreaseErle(std::array<float, kFftLengthBy2Plus1>& X2,
                               std::array<float, kFftLengthBy2Plus1>& E2,
                               std::array<float, kFftLengthBy2Plus1>& Y2,
                               ErleEstimator& estimator) {
  // Verifies that the ERLE estimate is properly increased to higher values.
  farendFrame(X2, E2, Y2, trueErle);

  for (size_t k = 0; k < 200; ++k) {
    estimator.Update(X2, Y2, E2);
  }
  VerifyErle(estimator.Erle(), estimator.ErleTimeDomain(), 8.f, 1.5f);
}

static void VerifyHoldErle(std::array<float, kFftLengthBy2Plus1>& X2,
                           std::array<float, kFftLengthBy2Plus1>& E2,
                           std::array<float, kFftLengthBy2Plus1>& Y2,
                           ErleEstimator& estimator) {
  nearendFrame(X2, E2, Y2);
  // Verifies that the ERLE is not immediately decreased during nearend activity
  for (size_t k = 0; k < 98; ++k) {
    estimator.Update(X2, Y2, E2);
  }
  VerifyErle(estimator.Erle(), estimator.ErleTimeDomain(), 8.f, 1.5f);
}

static void VerifyNotUpdateLowActivity(
    std::array<float, kFftLengthBy2Plus1>& X2,
    std::array<float, kFftLengthBy2Plus1>& E2,
    std::array<float, kFftLengthBy2Plus1>& Y2,
    ErleEstimator& estimator) {
  // Verifies that the ERLE estimate is is not updated for low-level render
  // signals.
  X2.fill(1000.f * 1000.f);
  Y2.fill(10 * E2[0]);
  for (size_t k = 0; k < 200; ++k) {
    estimator.Update(X2, Y2, E2);
  }
  VerifyErle(estimator.Erle(), estimator.ErleTimeDomain(), 1.f, 1.f);
}

// Verifies that the correct ERLE estimates are achieved.
TEST(ErleEstimator, Estimates) {
  std::array<float, kFftLengthBy2Plus1> X2;
  std::array<float, kFftLengthBy2Plus1> E2;
  std::array<float, kFftLengthBy2Plus1> Y2;

  ErleEstimator estimator(min_erle, max_erle_lf, max_erle_hf);

  VerifyIncreaseErle(X2, E2, Y2, estimator);
  VerifyHoldErle(X2, E2, Y2, estimator);
  VerifyVaryingEnvironment(X2, E2, Y2, estimator);
  VerifyNotUpdateLowActivity(X2, E2, Y2, estimator);
}
}  // namespace erle_unittest
}  // namespace webrtc
