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

void ProcessHighPassFilter(rtc::ArrayView<const float> x,
                           rtc::ArrayView<float> y,
                           rtc::ArrayView<float, 2> mem) {
  RTC_DCHECK_EQ(x.size(), y.size());
  for (size_t i = 0; i < y.size(); ++i) {
    float x_i = x[i];
    float y_i = x[i] + mem[0];
    mem[0] = mem[1] + (kBiQuadHighPassB[0] * static_cast<double>(x_i) -
                       kBiQuadHighPassA[0] * static_cast<double>(y_i));
    mem[1] = (kBiQuadHighPassB[1] * static_cast<double>(x_i) -
              kBiQuadHighPassA[1] * static_cast<double>(y_i));
    y[i] = y_i;
  }
}

float ComputeForwardFft(rtc::ArrayView<const float> samples,
                        rtc::ArrayView<const float> half_analysis_win,
                        const RealFourier* fft,
                        AlignedArray<float>* fft_input_buf,
                        AlignedArray<std::complex<float>>* fft_output_buf) {
  RTC_DCHECK(fft_input_buf);
  RTC_DCHECK(fft_output_buf);
  RTC_DCHECK(fft);
  RTC_DCHECK_EQ(2 * half_analysis_win.size(), samples.size());
  // Windowing and FFT in half-band.
  float tot_energy = 0.f;
  for (size_t i = 0; i < half_analysis_win.size(); ++i) {
    // Apply windowing.
    fft_input_buf->Row(0)[i] = half_analysis_win[i] * samples[i];
    fft_input_buf->Row(0)[samples.size() - i - 1] =
        half_analysis_win[i] * samples[samples.size() - i - 1];
    // Update total energy.
    tot_energy += fft_input_buf->Row(0)[i] * fft_input_buf->Row(0)[i];
    tot_energy += fft_input_buf->Row(0)[samples.size() - i - 1] *
                  fft_input_buf->Row(0)[samples.size() - i - 1];
  }
  // Clean the output buffer.
  // TODO(alessiob): Open a bug since this is not intuitive at all and it was
  // hard to find that cleaning is needed.
  for (size_t i = 0; i < fft_output_buf->cols(); ++i)
    fft_output_buf->Row(0)[i] = 0;
  fft->Forward(fft_input_buf->Row(0), fft_output_buf->Row(0));
  return tot_energy;
}

void ComputeBandEnergies(const AlignedArray<std::complex<float>>* fft_buf,
                         const float scaling,
                         rtc::ArrayView<const size_t> bands_indexes,
                         rtc::ArrayView<float> band_energies) {
  RTC_DCHECK_EQ(bands_indexes.size(), band_energies.size());
  RTC_DCHECK(fft_buf);
  RTC_DCHECK_LT(0.f, scaling);
  // Init.
  for (auto& v : band_energies)
    v = 0.f;
  RTC_DCHECK_EQ(0.f, band_energies[0]);
  size_t i;
  const size_t max_freq_bin_index = fft_buf->cols() - 1;
  for (i = 0; i < band_energies.size() - 1; ++i) {
    RTC_DCHECK_EQ(0.f, band_energies[i + 1]);
    RTC_DCHECK_GT(bands_indexes[i + 1], bands_indexes[i]);
    const size_t first_freq_bin = bands_indexes[i];
    const size_t last_freq_bin =
        std::min(max_freq_bin_index,
                 first_freq_bin + bands_indexes[i + 1] - bands_indexes[i] - 1);
    RTC_DCHECK_GE(last_freq_bin, first_freq_bin);
    const size_t band_size = last_freq_bin - first_freq_bin + 1;
    if (first_freq_bin >= last_freq_bin)
      break;
    // Compute the band energy using a triangular band with peak response at the
    // band boundary.
    for (size_t j = first_freq_bin; j <= last_freq_bin; ++j) {
      const float w = static_cast<float>(j - first_freq_bin) / band_size;
      const float freq_bin_energy = std::norm(fft_buf->Row(0)[j] * scaling);
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
      half_analysis_win_(ComputeHalfVorbisWindow<kHalfFrameSize / 2>()),
      fft_(RealFourier::Create(RealFourier::FftOrder(kHalfFrameSize))),
      fft_input_buf_(1, kHalfFrameSize, RealFourier::kFftBufferAlignment),
      fft_output_buf_(1,
                      RealFourier::ComplexLength(fft_->order()),
                      RealFourier::kFftBufferAlignment),
      bands_indexes_(ComputeOpusBandsIndexes(kHalfSampleRate, kHalfFftSize)),
      is_silence_(true) {
  RTC_DCHECK_EQ(kHalfFftSize / 2 + 1, fft_output_buf_.cols())
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
  ProcessHighPassFilter(samples, {samples_filtered},
                        {hp_filter_state_.data(), hp_filter_state_.size()});
  // Feed buffers.
  full_band_buf_.Push(samples);
  PushDecimated2xSamples(&half_band_buf_, samples);
  // Compute features.
  AnalyzePitch();
  ComputeForwardFft(half_band_buf_.GetBufferView(kHalfPitchMaxPeriod),
                    {half_analysis_win_}, fft_.get(), &fft_input_buf_,
                    &fft_output_buf_);
  ComputeBandEnergies(&fft_output_buf_, 1.f / kHalfFftSize, {bands_indexes_},
                      {band_energies_});
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
