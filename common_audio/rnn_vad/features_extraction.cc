/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/features_extraction.h"

#include <algorithm>
#include <complex>
#include <iostream>

#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/lp_residual.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr size_t kHalfFftSize = 256;  // Next power of 2 of |kHalfFrameSize|.
static_assert(kHalfFftSize / 2 < kHalfFrameSize &&
                  kHalfFrameSize <= kHalfFftSize,
              "Invalid FFT size.");

// Bi-quad high-pass filter parameters.
constexpr float kBiQuadHighPassA[2] = {-1.99599, 0.99600};
constexpr float kBiQuadHighPassB[2] = {-2, 1};

// Copies even samples from |src| and pushes them into |dst|.
void PushDecimated2xSamples(
    SequenceBuffer<float, kHalfBufSize, kHalfInputFrameSize>* dst,
    rtc::ArrayView<const float, kInputFrameSize> src) {
  RTC_DCHECK(dst);
  // Copy even samples from |src|.
  std::array<float, kHalfInputFrameSize> samples_decimated;
  for (size_t i = 0; i < samples_decimated.size(); ++i)
    samples_decimated[i] = src[2 * i];
  // Push decimated samples into |dst|.
  dst->Push({samples_decimated.data(), samples_decimated.size()});
}

}  // namespace

std::array<size_t, kNumOpusBands> ComputeOpusBandsIndexes(
    const size_t sample_rate,
    const size_t frame_size) {
  std::array<size_t, kNumOpusBands> indexes;
  for (size_t i = 0; i < kNumOpusBands; ++i)
    indexes[i] = kOpusBandsFrequencies[i] * frame_size / sample_rate;
  return indexes;
}

HighPassFilter::HighPassFilter() {
  mem_[0] = 0.f;
  mem_[1] = 0.f;
}

HighPassFilter::HighPassFilter(rtc::ArrayView<const float> initial_state) {
  RTC_CHECK_EQ(2, initial_state.size());
  mem_[0] = initial_state[0];
  mem_[1] = initial_state[1];
}

HighPassFilter::~HighPassFilter() = default;

void HighPassFilter::ProcessFrame(rtc::ArrayView<const float> x,
                                  rtc::ArrayView<float> y) {
  RTC_DCHECK_EQ(x.size(), y.size());
  for (size_t i = 0; i < y.size(); ++i) {
    float x_i = x[i];
    float y_i = x[i] + mem_[0];
    mem_[0] = mem_[1] + (kBiQuadHighPassB[0] * static_cast<double>(x_i) -
                         kBiQuadHighPassA[0] * static_cast<double>(y_i));
    mem_[1] = (kBiQuadHighPassB[1] * static_cast<double>(x_i) -
               kBiQuadHighPassA[1] * static_cast<double>(y_i));
    y[i] = y_i;
  }
}

void ComputeBandEnergies(rtc::ArrayView<const std::complex<float>> fft_coeffs,
                         const float scaling,
                         rtc::ArrayView<const size_t> bands_indexes,
                         rtc::ArrayView<float> band_energies) {
  RTC_DCHECK_EQ(bands_indexes.size(), band_energies.size());
  RTC_DCHECK_LT(0.f, scaling);
  // Init.
  for (auto& v : band_energies)
    v = 0.f;
  RTC_DCHECK_EQ(0.f, band_energies[0]);
  size_t i;
  const size_t max_freq_bin_index = fft_coeffs.size() - 1;
  for (i = 0; i < band_energies.size() - 1; ++i) {
    RTC_DCHECK_EQ(0.f, band_energies[i + 1]);
    RTC_DCHECK_GT(bands_indexes[i + 1], bands_indexes[i]);
    const size_t first_freq_bin = bands_indexes[i];
    const size_t last_freq_bin =
        std::min(max_freq_bin_index,
                 first_freq_bin + bands_indexes[i + 1] - bands_indexes[i] - 1);
    if (first_freq_bin >= last_freq_bin)
      break;
    const size_t band_size = last_freq_bin - first_freq_bin + 1;
    // Compute the band energy using a triangular band with peak response at the
    // band boundary.
    for (size_t j = first_freq_bin; j <= last_freq_bin; ++j) {
      const float w = static_cast<float>(j - first_freq_bin) / band_size;
      const float freq_bin_energy = std::norm(fft_coeffs[j] * scaling);
      band_energies[i] += (1.f - w) * freq_bin_energy;
      band_energies[i + 1] += w * freq_bin_energy;
    }
  }
  // Fix the first and the last bands (they only got half contribution).
  band_energies[0] *= 2.f;
  band_energies[band_energies.size() - 1] *= 2.f;
  // band_energies[i] *= 2.f;
}

RnnVadFeaturesExtractor::RnnVadFeaturesExtractor()
    : full_band_buf_(0.f),
      half_band_buf_(0.f),
      fft_(kHalfFrameSize),
      bands_indexes_(ComputeOpusBandsIndexes(kHalfSampleRate, kHalfFftSize)),
      is_silence_(true) {
  RTC_DCHECK_EQ(kHalfFftSize / 2 + 1, fft_.num_fft_points())
      << "Unexpected FFT size.";
  Reset();
}

RnnVadFeaturesExtractor::~RnnVadFeaturesExtractor() = default;

void RnnVadFeaturesExtractor::Reset() {
  feature_vector_.fill(0.f);
}

rtc::ArrayView<const float, kFeatureVectorSize>
RnnVadFeaturesExtractor::GetOutput() const {
  return {feature_vector_.data(), kFeatureVectorSize};
}

void RnnVadFeaturesExtractor::ComputeFeatures(
    rtc::ArrayView<const float, kInputFrameSize> samples) {
  // High-pass filter.
  // TODO(alessiob): This is originally done at 48 kHz, not 24 kHz.
  std::array<float, kInputFrameSize> samples_filtered;
  hpf_.ProcessFrame(samples, {samples_filtered});
  // Feed buffers.
  full_band_buf_.Push(samples);
  PushDecimated2xSamples(&half_band_buf_, samples);
  // Compute features.
  AnalyzePitch();
  fft_.ForwardFft(half_band_buf_.GetBufferView(kHalfPitchMaxPeriod));
  ComputeBandEnergies(fft_.GetFftOutputBufferView(), 1.f / kHalfFftSize,
                      {bands_indexes_}, {band_energies_});
  is_silence_ = false;
}

void RnnVadFeaturesExtractor::AnalyzePitch() {
  // Extract the LP residual (full-band).
  std::array<float, kBufSize> lp_residual;
  {
    auto buf_view = full_band_buf_.GetBufferView();
    float lpc_coeffs[kNumLpcCoefficients];
    rtc::ArrayView<float, kNumLpcCoefficients> lpc_coeffs_view(
        lpc_coeffs, kNumLpcCoefficients);
    ComputeInverseFilterCoefficients(lpc_coeffs_view, buf_view);
    ComputeLpResidual({lp_residual.data(), lp_residual.size()}, lpc_coeffs_view,
                      buf_view);
  }
  // Search pitch on the LP-residual.
  pitch_info_ =
      PitchSearch({lp_residual.data(), lp_residual.size()}, pitch_info_);
}

}  // namespace rnn_vad
}  // namespace webrtc
