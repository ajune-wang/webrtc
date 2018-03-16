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

constexpr size_t kHalfFftSize = 256;  // Next power of 2 of |kHalf20msFrameSize|.
static_assert(kHalfFftSize / 2 < kHalf20msFrameSize &&
                  kHalf20msFrameSize <= kHalfFftSize,
              "Invalid FFT size.");

// Bi-quad high-pass filter config (generated in Python).
// B, A = scipy.signal.iirfilter(2, 30/12000, btype='highpass')
// def PlotFilter(b, a):
//   w, h = signal.freqz(b, a, 1000)
//   fig = plt.figure()
//   ax = fig.add_subplot(111)
//   ax.plot(w, 20 * np.log10(abs(h)))
//   ax.set_xscale('log')
//   ax.set_title('frequency response')
//   ax.set_xlabel('Frequency [radians / second]')
//   ax.set_ylabel('Amplitude [dB]')
//   # ax.axis((10, 1000, -100, 10))
//   ax.grid(which='both', axis='both')
//   plt.show()
const BiQuadFilter::Config kHpfConfig24k(-1.98889291,
                                         0.98895425,
                                         0.99446179,
                                         -1.98892358,
                                         0.99446179);

// Copies even samples from |src| and pushes them into |dst|.
void PushDecimated2xSamples(
    SequenceBuffer<float, kHalfBufSize, kHalf10msFrameSize>* dst,
    rtc::ArrayView<const float, k10msFrameSize> src) {
  RTC_DCHECK(dst);
  // Copy even samples from |src|.
  std::array<float, kHalf10msFrameSize> samples_decimated;
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
  // TODO(alessiob): Check if replacing the line above with the one below leads
  // to better performance.
  // band_energies[i] *= 2.f;
}

RnnVadFeaturesExtractor::RnnVadFeaturesExtractor()
    : full_band_buf_(0.f),
      half_band_buf_(0.f),
      fft_(kHalf20msFrameSize),
      bands_indexes_(ComputeOpusBandsIndexes(kHalfSampleRate, kHalfFftSize)),
      band_energies_ring_buf_(0.f),
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
    rtc::ArrayView<const float, k10msFrameSize> samples) {
  // High-pass filter.
  // TODO(alessiob): This is originally done at 48 kHz, not 24 kHz.
  std::array<float, k10msFrameSize> samples_filtered;
  hpf_.ProcessFrame(samples, {samples_filtered});
  // Feed buffers.
  full_band_buf_.Push(samples);
  PushDecimated2xSamples(&half_band_buf_, samples);
  // Estimate pitch .
  AnalyzePitch();
  // Compute band energy coefficients for the reference frame and another one
  // extracted from the analysis buffer with a lag equal to the estimated pitch
  // period.
  // Reference frame.
  std::array<float, kNumOpusBands> band_energies_x;
  ComputeBandEnergyCoefficients(kHalfPitchMaxPeriod, {band_energies_x});
  // Lagged frame. The estimated pitch period is scaled since the half band
  // analysis buffer is used.
  std::array<float, kNumOpusBands> band_energies_p;
  ComputeBandEnergyCoefficients(kHalfPitchMaxPeriod - pitch_info_.period / 4,
                                {band_energies_p});
  // Compute features.
  // band_energies_ring_buf_.Push({});
  is_silence_ = false;
}

void RnnVadFeaturesExtractor::ComputeBandEnergyCoefficients(
    const size_t inverted_lag, rtc::ArrayView<float> output) {
  fft_.ForwardFft(half_band_buf_.GetBufferView(
      inverted_lag, kHalf20msFrameSize));
  ComputeBandEnergies(fft_.GetFftOutputBufferView(), 1.f / kHalfFftSize,
                      {bands_indexes_}, output);
  // TODO(alessiob): Dump the FFT result for debugging to visually check that
  // the estimated pitch period is valid. It is ok to dump inverted lags with
  // which one can recover the FFT coefficients.
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
