/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/spectral_features_internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// Number of FFT frequency bins covered by each band in the Opus scale at a
// sample rate of 24 kHz for 20 ms frames.
constexpr std::array<int, kOpusBands24kHz - 1> kOpusScaleNumBins24kHz20ms = {
    {4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8, 16, 16, 16, 24, 24, 32, 48}};

// Returns true if the values in |kOpusScaleNumBins24kHz20ms| match the Opus
// scale frequency boundaries.
bool ValidOpusScaleNumBins() {
  constexpr int kBandFrequencyBoundariesHz[kNumBands - 1] = {
      200,  400,  600,  800,  1000, 1200, 1400, 1600,  2000,  2400, 2800,
      3200, 4000, 4800, 5600, 6800, 8000, 9600, 12000, 15600, 20000};
  int prev = 0;
  for (size_t i = 0; i < kOpusScaleNumBins24kHz20ms.size(); ++i) {
    int boundary =
        kBandFrequencyBoundariesHz[i] * kFrameSize20ms24kHz / kSampleRate24kHz;
    if (kOpusScaleNumBins24kHz20ms[i] != (boundary - prev)) {
      return false;
    }
    prev = boundary;
  }
  return true;
}

std::vector<float> ComputeTriangularFiltersWeights() {
  const auto& v = kOpusScaleNumBins24kHz20ms;  // Alias.
  const size_t num_weights = std::accumulate(v.begin(), v.end(), 0);
  std::vector<float> weights(num_weights);
  size_t next_fft_coeff_index = 0;
  for (size_t band = 0; band < v.size(); ++band) {
    const size_t band_size = v[band];
    for (size_t j = 0; j < band_size; ++j) {
      weights[next_fft_coeff_index + j] = static_cast<float>(j) / band_size;
    }
    next_fft_coeff_index += band_size;
  }
  return weights;
}

// DCT scaling factor.
static_assert(
    kNumBands == 22,
    "kNumBands changed! Please update the value of kDctScalingFactor");
constexpr float kDctScalingFactor = 0.301511345f;  // sqrt(2 / kNumBands)

}  // namespace

BandFeaturesExtractor::BandFeaturesExtractor()
    : weights_(ComputeTriangularFiltersWeights()) {
  RTC_DCHECK(ValidOpusScaleNumBins())
      << "kOpusScaleNumBins24kHz20ms does not match the Opus scale.";
  RTC_DCHECK_LE(std::accumulate(kOpusScaleNumBins24kHz20ms.begin(),
                                kOpusScaleNumBins24kHz20ms.end(), 0.f),
                kFftSize20ms24kHz)
      << "The total number of frequency bins is greater than the number of FFT "
         "coefficients.";
}

BandFeaturesExtractor::~BandFeaturesExtractor() = default;

void BandFeaturesExtractor::ComputeBandEnergies(
    rtc::ArrayView<const std::complex<float>> fft,
    rtc::ArrayView<float, kOpusBands24kHz> bands_energy) const {
  RTC_DCHECK_EQ(fft.size(), kFftSize20ms24kHz);
  rtc::ArrayView<const float> weights(weights_);
  size_t next_fft_coeff_index = 0;
  bands_energy[0] = 0.f;
  for (size_t i = 0; i < kOpusScaleNumBins24kHz20ms.size(); ++i) {
    size_t band_size = kOpusScaleNumBins24kHz20ms[i];
    const auto x = fft.subview(next_fft_coeff_index, band_size);
    // A band may end beyond the Nyquist frequency; if so, trim the weights
    // sub-vector.
    RTC_DCHECK_GT(bands_energy.size(), i + 1);
    bands_energy[i + 1] = 0.f;
    const auto w = weights.subview(next_fft_coeff_index, x.size());
    for (size_t k = 0; k < x.size(); ++k) {
      float v = std::norm(x[k]);
      bands_energy[i] += (1.f - w[k]) * v;
      bands_energy[i + 1] += w[k] * v;
    }
    next_fft_coeff_index += band_size;
  }
  bands_energy[0] *= 2.f;  // The first band only gets half contribution.
}

void BandFeaturesExtractor::ComputeSpectralCrossCorrelation(
    rtc::ArrayView<const std::complex<float>> x_fft,
    rtc::ArrayView<const std::complex<float>> y_fft,
    rtc::ArrayView<float, kOpusBands24kHz> cross_corr) const {
  RTC_DCHECK_EQ(x_fft.size(), kFftSize20ms24kHz);
  RTC_DCHECK_EQ(y_fft.size(), kFftSize20ms24kHz);
  rtc::ArrayView<const float> weights(weights_);
  size_t next_fft_coeff_index = 0;
  cross_corr[0] = 0.f;
  for (size_t i = 0; i < kOpusScaleNumBins24kHz20ms.size(); ++i) {
    size_t band_size = kOpusScaleNumBins24kHz20ms[i];
    const auto x = x_fft.subview(next_fft_coeff_index, band_size);
    const auto y = y_fft.subview(next_fft_coeff_index, band_size);
    RTC_DCHECK_EQ(x.size(), y.size());
    // A band may end beyond the Nyquist frequency; if so, trim the weights
    // sub-vector.
    RTC_DCHECK_GT(cross_corr.size(), i + 1);
    cross_corr[i + 1] = 0.f;
    const auto w = weights.subview(next_fft_coeff_index, x.size());
    for (size_t k = 0; k < x.size(); ++k) {
      float v = x[k].real() * y[k].real() + x[k].imag() * y[k].imag();
      cross_corr[i] += (1.f - w[k]) * v;
      cross_corr[i + 1] += w[k] * v;
    }
    next_fft_coeff_index += band_size;
  }
  cross_corr[0] *= 2.f;  // The first band only gets half contribution.
}

void ComputeLogBandEnergiesCoefficients(
    rtc::ArrayView<const float, kNumBands> bands_energy,
    rtc::ArrayView<float, kNumBands> log_bands_energy) {
  float log_max = -2.f;
  float follow = -2.f;
  for (size_t i = 0; i < bands_energy.size(); ++i) {
    log_bands_energy[i] = std::log10(1e-2f + bands_energy[i]);
    // Smoothing across frequency bands.
    log_bands_energy[i] =
        std::max(log_max - 7.f, std::max(follow - 1.5f, log_bands_energy[i]));
    log_max = std::max(log_max, log_bands_energy[i]);
    follow = std::max(follow - 1.5f, log_bands_energy[i]);
  }
}

std::array<float, kNumBands * kNumBands> ComputeDctTable() {
  std::array<float, kNumBands * kNumBands> dct_table;
  const double k = std::sqrt(0.5);
  for (size_t i = 0; i < kNumBands; ++i) {
    for (size_t j = 0; j < kNumBands; ++j)
      dct_table[i * kNumBands + j] = std::cos((i + 0.5) * j * kPi / kNumBands);
    dct_table[i * kNumBands] *= k;
  }
  return dct_table;
}

void ComputeDct(rtc::ArrayView<const float, kNumBands> in,
                rtc::ArrayView<const float, kNumBands * kNumBands> dct_table,
                rtc::ArrayView<float> out) {
  RTC_DCHECK_NE(in.data(), out.data()) << "In-place DCT is not supported.";
  RTC_DCHECK_LE(1, out.size());
  RTC_DCHECK_LE(out.size(), in.size());
  std::fill(out.begin(), out.end(), 0.f);
  for (size_t i = 0; i < out.size(); ++i) {
    for (size_t j = 0; j < in.size(); ++j) {
      out[i] += in[j] * dct_table[j * in.size() + i];
    }
    out[i] *= kDctScalingFactor;
  }
}

}  // namespace rnn_vad
}  // namespace webrtc
