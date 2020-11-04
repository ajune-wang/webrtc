/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/pitch_search.h"

#include <array>
#include <cstddef>

#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr int kYenergySize = kMaxPitch24kHz + 1;

}  // namespace

PitchEstimator::PitchEstimator()
    : y_energy_(kYenergySize, 0.f),
      decimated_pitch_buffer_(kBufSize12kHz),
      auto_correlation_(kNumInvertedLags12kHz) {}

PitchEstimator::~PitchEstimator() = default;

PitchInfo PitchEstimator::Estimate(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer) {
  rtc::ArrayView<float, kBufSize12kHz> decimated_pitch_buffer_view(
      decimated_pitch_buffer_.data(), kBufSize12kHz);
  RTC_DCHECK_EQ(decimated_pitch_buffer_.size(),
                decimated_pitch_buffer_view.size());
  rtc::ArrayView<float, kNumInvertedLags12kHz> auto_correlation_view(
      auto_correlation_.data(), kNumInvertedLags12kHz);
  RTC_DCHECK_EQ(auto_correlation_.size(), auto_correlation_view.size());

  // Perform the initial pitch search at 12 kHz.
  Decimate2x(pitch_buffer, decimated_pitch_buffer_view);
  auto_corr_calculator_.ComputeOnPitchBuffer(decimated_pitch_buffer_view,
                                             auto_correlation_view);
  CandidatePitchPeriods pitch_candidates_inverted_lags =
      FindBestPitchPeriods12kHz(auto_correlation_view,
                                decimated_pitch_buffer_view);

  // Refine the initial pitch period estimation from 12 kHz to 24 kHz.
  // Step 1: pre-compute frame energies at 24 kHz.
  rtc::ArrayView<float, kYenergySize> y_energy_view(y_energy_.data(),
                                                    kYenergySize);
  RTC_DCHECK_EQ(y_energy_.size(), y_energy_view.size());
  ComputeSlidingFrameSquareEnergies(pitch_buffer, y_energy_view);
  // The refinement is done using the pitch buffer that contains 24 kHz samples.
  // Therefore, adapt the inverted lags in |pitch_candidates_inv_lags| from 12
  // to 24 kHz.
  pitch_candidates_inverted_lags.best *= 2;
  pitch_candidates_inverted_lags.second_best *= 2;
  const int pitch_inv_lag_48kHz = RefinePitchPeriod48kHz(
      pitch_buffer, y_energy_view, pitch_candidates_inverted_lags);
  // Step 2: look for stronger harmonics to find the final pitch period and its
  // gain.
  RTC_DCHECK_LT(pitch_inv_lag_48kHz, kMaxPitch48kHz);
  last_pitch_48kHz_ = CheckLowerPitchPeriodsAndComputePitchGain(
      pitch_buffer, y_energy_view, kMaxPitch48kHz - pitch_inv_lag_48kHz,
      last_pitch_48kHz_);
  return last_pitch_48kHz_;
}

}  // namespace rnn_vad
}  // namespace webrtc
