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
#include <cmath>
#include <complex>

namespace webrtc {
namespace rnn_vad {
namespace {

const double kPi = std::acos(-1);
const int kOpusBandsFrequencies[kNumOpusBands] = {
    0,    200,  400,  600,  800,  1000, 1200, 1400, 1600,  2000,  2400,
    2800, 3200, 4000, 4800, 5600, 6800, 8000, 9600, 12000, 15600, 20000};
constexpr size_t kNumLpcCoefficients = 5;

// Computes the first half of the Vorbis window.
template <size_t S>
std::array<float, S> ComputeHalfVorbisWindow() {
  std::array<float, S> half_window;
  for (size_t i = 0; i < S; ++i)
    half_window[i] = std::sin(0.5 * kPi * std::sin(0.5 * kPi * (i + 0.5) / S) *
                              std::sin(0.5 * kPi * (i + 0.5) / S));
  return half_window;
}

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

// Computes FFT indexes corresponding to Opus bands.
std::array<float, kNumOpusBands> ComputeOpusBandsIndexes(
    const size_t sample_rate,
    const size_t frame_size) {
  std::array<float, kNumOpusBands> indexes;
  for (size_t i = 0; i < kNumOpusBands; ++i)
    indexes[i] = kOpusBandsFrequencies[i] * frame_size / sample_rate;
  return indexes;
}

void ComputeInverseFilterCoefficients(rtc::ArrayView<float> coeffs,
                                      rtc::ArrayView<const float> x) {
  // TODO(alessiob): Implement.
}

void DenoiseInverseFilterCoefficients(rtc::ArrayView<float> coeffs) {
  // TODO(alessiob): Implement.
}

void ComputeLpResidual(rtc::ArrayView<float, kHalfBufSize> y,
                       rtc::ArrayView<const float> coeffs,
                       rtc::ArrayView<const float> x) {
  // TODO(alessiob): Implement.
}

}  // namespace

RnnVadFeaturesExtractor::RnnVadFeaturesExtractor()
    : full_band_buf_(0.f),
      half_band_buf_(0.f),
      half_analysis_win_(ComputeHalfVorbisWindow<kHalfFrameSize / 2>()),
      fft_(RealFourier::Create(RealFourier::FftOrder(kHalfFrameSize))),
      fft_input_buf_(1, kHalfFrameSize, RealFourier::kFftBufferAlignment),
      fft_output_buf_(1,
                      RealFourier::ComplexLength(fft_->order()),
                      RealFourier::kFftBufferAlignment),
      bands_indexes_(ComputeOpusBandsIndexes(kHalfSampleRate, kHalfFrameSize)),
      is_silence_(true) {
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
  // Feed buffers.
  full_band_buf_.Push(samples);
  PushDecimated2xSamples(&half_band_buf_, samples);
  // Compute features.
  AnalyzeFrame();
  AnalyzePitch();
  is_silence_ = false;
}

void RnnVadFeaturesExtractor::AnalyzeFrame() {
  // Windowing and FFT in half-band.
  const auto samples = half_band_buf_.GetBufferView(kPitchMaxPeriod / 2);
  for (size_t i = 0; i < half_analysis_win_.size(); ++i) {
    fft_input_buf_.Row(0)[i] = half_analysis_win_[i] * samples[i];
    fft_input_buf_.Row(0)[kHalfFrameSize - i - 1] =
        half_analysis_win_[i] * samples[kHalfFrameSize - i - 1];
  }
  fft_->Forward(fft_input_buf_.Row(0), fft_output_buf_.Row(0));

  // Compute band energy using triangular bands with peak response at the bands
  // boundaries.
  band_energies_.fill(0.f);
  size_t i;
  for (i = 0; i < kNumOpusBands - 1; ++i) {
    const size_t band_size = bands_indexes_[i + 1] - bands_indexes_[i];
    const size_t first_freq_bin = bands_indexes_[i];
    const size_t last_freq_bin =
        std::min(first_freq_bin + band_size, kHalfFrameSize / 2);
    if (first_freq_bin >= last_freq_bin)
      break;
    for (size_t j = first_freq_bin; j < last_freq_bin; ++j) {
      const float w = static_cast<float>(j) / band_size;
      const float freq_bin_energy = std::norm(fft_output_buf_.Row(0)[j]);
      band_energies_[i] += (1.f - w) * freq_bin_energy;
      band_energies_[i + 1] += w * freq_bin_energy;
    }
  }
  // Fix the first and the last bands (they only got half contribution).
  band_energies_[0] *= 2;
  band_energies_[i] *= 2;
}

void RnnVadFeaturesExtractor::AnalyzePitch() {
  // Extract the LP residual.
  std::array<float, kHalfBufSize> lp_residual;
  {
    auto buf_view = half_band_buf_.GetBufferView();
    float lpc_coeffs[kNumLpcCoefficients];
    rtc::ArrayView<float> lpc_coeffs_view(lpc_coeffs, kNumLpcCoefficients);
    ComputeInverseFilterCoefficients(lpc_coeffs_view, buf_view);
    DenoiseInverseFilterCoefficients(lpc_coeffs_view);
    ComputeLpResidual({lp_residual.data(), lp_residual.size()}, lpc_coeffs_view,
                      buf_view);
  }

  // Search best and second best pitch candidates.

  // Refine pitch estimations in full band.

  // Remove doubling and compute pitch gain.
}

}  // namespace rnn_vad
}  // namespace webrtc
