/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/pitch_search_internal.h"

#include <array>
#include <tuple>

#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace test {
namespace {

constexpr int kTestPitchPeriodsLow = 3 * kMinPitch48kHz / 2;
constexpr int kTestPitchPeriodsHigh = (3 * kMinPitch48kHz + kMaxPitch48kHz) / 2;

constexpr float kTestPitchStrengthLow = 0.35f;
constexpr float kTestPitchStrengthHigh = 0.75f;

}  // namespace

class PitchSearchInternalParametrization
    : public ::testing::TestWithParam<Optimization> {};

// Checks that the frame-wise sliding square energy function produces output
// within tolerance given test input data.
TEST_P(PitchSearchInternalParametrization,
       ComputeSlidingFrameSquareEnergies24kHzWithinTolerance) {
  const Optimization optimization = GetParam();
  if (!IsOptimizationAvailable(optimization)) {
    return;
  }

  PitchTestData test_data;
  std::array<float, kNumPitchBufSquareEnergies> computed_output;
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  ComputeSlidingFrameSquareEnergies24kHz(test_data.GetPitchBufView(),
                                         computed_output, optimization);
  auto square_energies_view = test_data.GetPitchBufSquareEnergiesView();
  ExpectNearAbsolute({square_energies_view.data(), square_energies_view.size()},
                     computed_output, 1e-3f);
}

// Checks that the estimated pitch period is bit-exact given test input data.
TEST_P(PitchSearchInternalParametrization,
       ComputePitchPeriod12kHzBitExactness) {
  const Optimization optimization = GetParam();
  if (!IsOptimizationAvailable(optimization)) {
    return;
  }

  PitchTestData test_data;
  std::array<float, kBufSize12kHz> pitch_buf_decimated;
  Decimate2x(test_data.GetPitchBufView(), pitch_buf_decimated);
  CandidatePitchPeriods pitch_candidates;
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  auto auto_corr_view = test_data.GetPitchBufAutoCorrCoeffsView();
  pitch_candidates = ComputePitchPeriod12kHz(pitch_buf_decimated,
                                             auto_corr_view, optimization);
  EXPECT_EQ(pitch_candidates.best, 140);
  EXPECT_EQ(pitch_candidates.second_best, 142);
}

// Checks that the refined pitch period is bit-exact given test input data.
TEST_P(PitchSearchInternalParametrization,
       ComputePitchPeriod48kHzBitExactness) {
  const Optimization optimization = GetParam();
  if (!IsOptimizationAvailable(optimization)) {
    return;
  }

  PitchTestData test_data;
  std::vector<float> y_energy(kRefineNumLags24kHz);
  rtc::ArrayView<float, kRefineNumLags24kHz> y_energy_view(y_energy.data(),
                                                           kRefineNumLags24kHz);
  ComputeSlidingFrameSquareEnergies24kHz(test_data.GetPitchBufView(),
                                         y_energy_view, optimization);
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  EXPECT_EQ(
      ComputePitchPeriod48kHz(test_data.GetPitchBufView(), y_energy_view,
                              /*pitch_candidates=*/{280, 284}, optimization),
      560);
  EXPECT_EQ(
      ComputePitchPeriod48kHz(test_data.GetPitchBufView(), y_energy_view,
                              /*pitch_candidates=*/{260, 284}, optimization),
      568);
}

INSTANTIATE_TEST_SUITE_P(RnnVadTest,
                         PitchSearchInternalParametrization,
                         ::testing::Values(Optimization::kNone,
                                           Optimization::kAvx2));

class PitchCandidatesParametrization
    : public ::testing::TestWithParam<
          std::tuple<CandidatePitchPeriods, Optimization>> {
 protected:
  CandidatePitchPeriods GetPitchCandidates() const {
    return std::get<0>(GetParam());
  }
  CandidatePitchPeriods GetSwappedPitchCandidates() const {
    CandidatePitchPeriods candidate = GetPitchCandidates();
    return {candidate.second_best, candidate.best};
  }
  Optimization GetOptimization() const { return std::get<1>(GetParam()); }
};

// Checks that the result of `ComputePitchPeriod48kHz()` does not depend on the
// order of the input pitch candidates.
TEST_P(PitchCandidatesParametrization,
       ComputePitchPeriod48kHzOrderDoesNotMatter) {
  if (!IsOptimizationAvailable(GetOptimization())) {
    return;
  }

  PitchTestData test_data;
  std::vector<float> y_energy(kRefineNumLags24kHz);
  rtc::ArrayView<float, kRefineNumLags24kHz> y_energy_view(y_energy.data(),
                                                           kRefineNumLags24kHz);
  ComputeSlidingFrameSquareEnergies24kHz(test_data.GetPitchBufView(),
                                         y_energy_view, GetOptimization());
  EXPECT_EQ(
      ComputePitchPeriod48kHz(test_data.GetPitchBufView(), y_energy_view,
                              GetPitchCandidates(), GetOptimization()),
      ComputePitchPeriod48kHz(test_data.GetPitchBufView(), y_energy_view,
                              GetSwappedPitchCandidates(), GetOptimization()));
}

INSTANTIATE_TEST_SUITE_P(
    RnnVadTest,
    PitchCandidatesParametrization,
    ::testing::Combine(
        ::testing::Values(CandidatePitchPeriods{0, 2},
                          CandidatePitchPeriods{260, 284},
                          CandidatePitchPeriods{280, 284},
                          CandidatePitchPeriods{kInitialNumLags24kHz - 2,
                                                kInitialNumLags24kHz - 1}),
        ::testing::Values(Optimization::kNone, Optimization::kAvx2)));

struct ExtendedPitchPeriodSearchParameters {
  int initial_pitch_period;
  PitchInfo last_pitch;
  PitchInfo expected_pitch;
  Optimization optimization;
};

class ExtendedPitchPeriodSearchParametrizaion
    : public ::testing::TestWithParam<ExtendedPitchPeriodSearchParameters> {};

// Checks that the computed pitch period is bit-exact and that the computed
// pitch strength is within tolerance given test input data.
TEST_P(ExtendedPitchPeriodSearchParametrizaion,
       PeriodBitExactnessGainWithinTolerance) {
  const ExtendedPitchPeriodSearchParameters params = GetParam();
  if (!IsOptimizationAvailable(params.optimization)) {
    return;
  }

  PitchTestData test_data;
  std::vector<float> y_energy(kRefineNumLags24kHz);
  rtc::ArrayView<float, kRefineNumLags24kHz> y_energy_view(y_energy.data(),
                                                           kRefineNumLags24kHz);
  ComputeSlidingFrameSquareEnergies24kHz(test_data.GetPitchBufView(),
                                         y_energy_view, params.optimization);
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  const auto computed_output = ComputeExtendedPitchPeriod48kHz(
      test_data.GetPitchBufView(), y_energy_view, params.initial_pitch_period,
      params.last_pitch, params.optimization);
  EXPECT_EQ(params.expected_pitch.period, computed_output.period);
  EXPECT_NEAR(params.expected_pitch.strength, computed_output.strength, 1e-6f);
}

std::vector<ExtendedPitchPeriodSearchParameters>
CreateExtendedPitchPeriodSearchParameters() {
  std::vector<ExtendedPitchPeriodSearchParameters> v;
  constexpr Optimization kOptimizations[] = {Optimization::kNone,
                                             Optimization::kAvx2};
  for (Optimization optimization : kOptimizations) {
    for (int last_pitch_period :
         {kTestPitchPeriodsLow, kTestPitchPeriodsHigh}) {
      for (float last_pitch_strength :
           {kTestPitchStrengthLow, kTestPitchStrengthHigh}) {
        v.push_back({kTestPitchPeriodsLow,
                     {last_pitch_period, last_pitch_strength},
                     {91, -0.0188608f},
                     optimization});
        v.push_back({kTestPitchPeriodsHigh,
                     {last_pitch_period, last_pitch_strength},
                     {475, -0.0904344f},
                     optimization});
      }
    }
  }
  return v;
}

INSTANTIATE_TEST_SUITE_P(
    RnnVadTest,
    ExtendedPitchPeriodSearchParametrizaion,
    ::testing::ValuesIn(CreateExtendedPitchPeriodSearchParameters()));

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
