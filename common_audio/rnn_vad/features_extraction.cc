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
#include <numeric>

#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/lp_residual.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// Parameters for packing values into feature vectors.
constexpr size_t kNumBandCorrCoeffs = 6;
constexpr size_t kNumBandEnergyCoeffDeltas = 6;

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

// Helper function that iterates through frequency bands and computes
// coefficients for triangular bands with peak response at each band boundary.
void ComputeBandCoefficients(std::function<float(const size_t)> functor,
                             rtc::ArrayView<const size_t> band_boundaries,
                             const size_t max_freq_bin_index,
                             rtc::ArrayView<float> coefficients) {
  for (auto& v : coefficients)
    v = 0.f;
  for (size_t i = 0; i < coefficients.size() - 1; ++i) {
    RTC_DCHECK_EQ(0.f, coefficients[i + 1]);
    RTC_DCHECK_GT(band_boundaries[i + 1], band_boundaries[i]);
    const size_t first_freq_bin = band_boundaries[i];
    const size_t last_freq_bin =
        std::min(max_freq_bin_index, first_freq_bin + band_boundaries[i + 1] -
                                         band_boundaries[i] - 1);
    if (first_freq_bin >= last_freq_bin)
      break;
    const size_t band_size = last_freq_bin - first_freq_bin + 1;
    // Compute the band coefficient using a triangular band with peak response
    // at the band boundary.
    for (size_t j = first_freq_bin; j <= last_freq_bin; ++j) {
      const float w = static_cast<float>(j - first_freq_bin) / band_size;
      const float coefficient = functor(j);
      coefficients[i] += (1.f - w) * coefficient;
      coefficients[i + 1] += w * coefficient;
    }
  }
  // Fix the first and the last bands (they only got half contribution).
  coefficients[0] *= 2.f;
  coefficients[coefficients.size() - 1] *= 2.f;
  // TODO(alessiob): Check if replacing the line above with the one below leads
  // to better performance.
  // coefficients[i] *= 2.f;
}

// DCT table for arrays having size equal to |kNumOpusBands|.
std::array<float, kNumOpusBands * kNumOpusBands> ComputeDctTable() {
  std::array<float, kNumOpusBands * kNumOpusBands> dct_table;
  for (size_t i = 0; i < kNumOpusBands; ++i) {
    for (size_t j = 0; j < kNumOpusBands; ++j)
      dct_table[i * kNumOpusBands + j] =
          std::cos((i + 0.5) * j * kPi / kNumOpusBands);
    dct_table[i * kNumOpusBands] *= std::sqrt(0.5);
  }
  return dct_table;
}

// Normalize and pack features into the final feature vector.
// |band_energy_coeffs_curr| are the band energy coefficients for the current
// frame, whereas |band_energy_coeffs_prev1| and |band_energy_coeffs_prev2| for
// the last and the second last frames respectively. |band_corr_coeffs| are the
// band correlation coefficients for the current frame. |pitch_period| is the
// estimated pitch period at 2x sample rate and |spectral_variability| is a
// scalar score. The features are packed into |feature_vector|.
void WriteFeatureVector(
    rtc::ArrayView<const float> band_energy_coeffs_curr,
    rtc::ArrayView<const float> band_energy_coeffs_prev1,
    rtc::ArrayView<const float> band_energy_coeffs_prev2,
    rtc::ArrayView<const float> band_corr_coeffs,
    const int pitch_period,
    const float spectral_variability,
    rtc::ArrayView<float, kFeatureVectorSize> feature_vector) {
  size_t offset = 0;
  // Band energy coefficients (average, kernel: [1, 1, 1]).
  for (size_t i = 0; i < kNumOpusBands; ++i) {
    feature_vector[offset + i] = band_energy_coeffs_curr[i] +
                                 band_energy_coeffs_prev1[i] +
                                 band_energy_coeffs_prev2[i];
  }
  feature_vector[offset] -= 12;
  feature_vector[offset + 1] -= 4;
  offset += kNumOpusBands;
  // Band energy coefficients (first derivative, kernel: [1, 0, - 1]).
  for (size_t i = 0; i < kNumBandEnergyCoeffDeltas; ++i) {
    feature_vector[offset + i] =
        band_energy_coeffs_curr[i] - band_energy_coeffs_prev2[i];
  }
  offset += kNumBandEnergyCoeffDeltas;
  // Band energy coefficients (second derivative, Laplacian kernel: [1, -2, 1]).
  for (size_t i = 0; i < kNumBandEnergyCoeffDeltas; ++i) {
    feature_vector[offset + i] = band_energy_coeffs_curr[i] -
                                 2 * band_energy_coeffs_prev1[i] +
                                 band_energy_coeffs_prev2[i];
  }
  offset += kNumBandEnergyCoeffDeltas;
  // Band correlation coefficients.
  for (size_t i = 0; i < kNumBandCorrCoeffs; ++i) {
    feature_vector[offset + i] = band_corr_coeffs[i];
  }
  feature_vector[offset] -= 1.3f;
  feature_vector[offset + 1] -= 0.9f;
  offset += kNumBandCorrCoeffs;
  // Pitch period (normalized).
  feature_vector[offset] = 0.01f * (pitch_period - 300);
  offset++;
  // Spectral variability (normalized).
  feature_vector[offset] =
      spectral_variability / kBandEnergyCoeffsHistorySize - 2.1f;
  offset++;
  RTC_DCHECK_EQ(kFeatureVectorSize, offset);
}

// Compute log band energy coefficients.
void ComputeLogBandEnergiesCoefficients(
    rtc::ArrayView<const float> band_energy_coeffs,
    rtc::ArrayView<float> log_band_energy_coeffs) {
  // TODO(alessiob): Describe smoothing with log_max and follow.
  RTC_DCHECK_EQ(band_energy_coeffs.size(), log_band_energy_coeffs.size());
  float log_max = -2.f;
  float follow = -2;
  for (size_t i = 0; i < band_energy_coeffs.size(); ++i) {
    log_band_energy_coeffs[i] = std::log10(1e-2f + band_energy_coeffs[i]);
    log_band_energy_coeffs[i] = std::max(
        log_max - 7.f, std::max(follow - 1.5f, log_band_energy_coeffs[i]));
    log_max = std::max(log_max, log_band_energy_coeffs[i]);
    follow = std::max(follow - 1.5f, log_band_energy_coeffs[i]);
  }
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
                         rtc::ArrayView<const size_t> band_boundaries,
                         rtc::ArrayView<float> band_energies) {
  RTC_DCHECK_EQ(band_boundaries.size(), band_energies.size());
  const float scaling = 1.f / (2 * fft_coeffs.size() - 2);
  auto functor = [scaling, fft_coeffs](const size_t freq_bin_index) {
    return std::norm(fft_coeffs[freq_bin_index] * scaling);
  };
  ComputeBandCoefficients(functor, band_boundaries, fft_coeffs.size() - 1,
                          band_energies);
}

void ComputeBandWiseCorrelationCoefficients(
    rtc::ArrayView<const std::complex<float>> fft_ref,
    rtc::ArrayView<const float> band_energy_coeffs_ref,
    rtc::ArrayView<const std::complex<float>> fft_lagged,
    rtc::ArrayView<const float> band_energy_coeffs_lagged,
    rtc::ArrayView<const size_t> band_boundaries,
    rtc::ArrayView<float> band_correlations) {
  RTC_DCHECK_EQ(fft_ref.size(), fft_lagged.size());
  RTC_DCHECK_EQ(band_energy_coeffs_ref.size(),
                band_energy_coeffs_lagged.size());
  RTC_DCHECK_EQ(band_energy_coeffs_ref.size(), band_boundaries.size());
  RTC_DCHECK_LE(1, band_correlations.size());
  RTC_DCHECK_EQ(band_correlations.size(), band_boundaries.size());
  const float scaling = 1.f / (2 * fft_ref.size() - 2);
  auto functor = [scaling, fft_ref, fft_lagged](const size_t freq_bin_index) {
    return (fft_ref[freq_bin_index].real() * fft_lagged[freq_bin_index].real() +
            fft_ref[freq_bin_index].imag() *
                fft_lagged[freq_bin_index].imag()) *
           scaling * scaling;
  };
  ComputeBandCoefficients(functor, band_boundaries, fft_ref.size() - 1,
                          band_correlations);
  // Normalize.
  for (size_t i = 0; i < band_correlations.size(); ++i) {
    band_correlations[i] = band_correlations[i] /
                           std::sqrt(0.001f + band_energy_coeffs_ref[i] *
                                                  band_energy_coeffs_lagged[i]);
  }
}

void ComputeDct(rtc::ArrayView<const float> in,
                rtc::ArrayView<const float> dct_table,
                const float dct_scaling_factor_,
                rtc::ArrayView<float> out) {
  RTC_DCHECK_NE(in.data(), out.data()) << "In-place DCT is not allowed.";
  RTC_DCHECK_EQ(in.size() * in.size(), dct_table.size());
  RTC_DCHECK_LE(1, out.size());
  RTC_DCHECK_LE(out.size(), in.size());
  RTC_DCHECK_EQ(std::sqrt(2.f / in.size()), dct_scaling_factor_);
  for (size_t i = 0; i < out.size(); ++i) {
    out[i] = 0.f;
    for (size_t j = 0; j < in.size(); ++j) {
      out[i] += in[j] * dct_table[i * in.size() + j];
    }
    out[i] *= dct_scaling_factor_;
  }
}

float ComputeSpectralVariability(
    const RingBuffer<float, kNumOpusBands, kBandEnergyCoeffsHistorySize>*
        ring_buffer) {
  // TODO(alessiob): Describe what this function does.
  // TODO(alessiob): Maybe reduce complexity by adding memory.
  float spec_variability = 0.f;
  for (size_t i = 0; i < kBandEnergyCoeffsHistorySize; ++i) {
    float min_dist = 1e15f;
    for (size_t j = 0; j < kBandEnergyCoeffsHistorySize; ++j) {
      float dist = 0.f;
      for (size_t k = 0; k < kNumOpusBands; ++k) {
        float c =
            ring_buffer->GetArrayView(i)[k] - ring_buffer->GetArrayView(j)[k];
        dist += c * c;
      }
      if (j != i)
        min_dist = std::min(min_dist, dist);
    }
    spec_variability += min_dist;
  }
  return spec_variability;
}

RnnVadFeaturesExtractor::RnnVadFeaturesExtractor()
    : full_band_buf_(0.f),
      half_band_buf_(0.f),
      fft_(kHalf20msFrameSize),
      bands_indexes_(
          ComputeOpusBandsIndexes(kHalfSampleRate, kHalf20msFftLenght)),
      log_band_energies_ring_buf_(0.f),
      dct_table_(ComputeDctTable()),
      dct_scaling_factor_(std::sqrt(2.f / kNumOpusBands)),
      is_silence_(true) {
  RTC_CHECK_EQ(kHalf20msFftLenght, fft_.fft_length());
  RTC_CHECK_EQ(kHalf20msNumFftPoints, fft_.num_fft_points());
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
  // Pre-processing.
  std::array<float, k10msFrameSize> samples_filtered;
  rtc::ArrayView<float, k10msFrameSize> samples_filtered_view(
      samples_filtered.data(), samples_filtered.size());
  hpf_.ProcessFrame(samples, {samples_filtered});
  // Feed buffers.
  full_band_buf_.Push(samples_filtered_view);
  PushDecimated2xSamples(&half_band_buf_, samples_filtered_view);
  // Update pitch estimation.
  UpdatePitchEstimation();
  // Compute band energy coefficients for the reference frame.
  std::array<float, kNumOpusBands> band_energies_ref;
  ComputeFftAndBandEnergyCoefficients(kHalfPitchMaxPeriod, {frame_fft_ref_},
                                      {band_energies_ref});
  // Detect silence, return if found.
  const float tot_energy =
      std::accumulate(band_energies_ref.begin(), band_energies_ref.end(), 0.f);
  is_silence_ = tot_energy < 0.04;
  if (is_silence_)
    return;
  // Compute band energy coefficients for a frame lagged by the estimated
  // pitch period, which is scaled since the energy coefficients are computed in
  // half band (and the pitch is at 2x sample rate).
  std::array<float, kNumOpusBands> band_energies_lagged;
  ComputeFftAndBandEnergyCoefficients(
      kHalfPitchMaxPeriod - pitch_info_.period / 4, {frame_fft_lagged_},
      {band_energies_lagged});
  // Compute band-wise correlation coefficients.
  std::array<float, kNumOpusBands> band_corr_coeffs;
  ComputeBandWiseCorrelationCoefficients(
      {frame_fft_ref_}, {band_energies_ref}, {frame_fft_lagged_},
      {band_energies_lagged}, {bands_indexes_}, {band_corr_coeffs});
  // Decorrelate band-wise correlation coefficients via DCT.
  std::array<float, kNumBandCorrCoeffs> band_corr_coeffs_decorrelated;
  ComputeDct({band_corr_coeffs}, {dct_table_}, dct_scaling_factor_,
             {band_corr_coeffs_decorrelated});
  // Log of the band energies.
  std::array<float, kNumOpusBands> log_band_energy_coeffs;
  ComputeLogBandEnergiesCoefficients({band_energies_ref},
                                     {log_band_energy_coeffs});
  // Decorrelate band-wise log energy coefficients via DCT.
  std::array<float, kNumOpusBands> log_band_energy_coeffs_decorrelated;
  ComputeDct({log_band_energy_coeffs}, {dct_table_}, dct_scaling_factor_,
             {log_band_energy_coeffs_decorrelated});
  // Update the ring buffer. Note that this is done only when silence is not
  // detected.
  log_band_energies_ring_buf_.Push(
      {log_band_energy_coeffs_decorrelated.data(),
       log_band_energy_coeffs_decorrelated.size()});
  // Spectral variability.
  const float spectral_variability =
      ComputeSpectralVariability(&log_band_energies_ring_buf_);
  // Normalize and pack features into the final feature vector.
  WriteFeatureVector(log_band_energies_ring_buf_.GetArrayView(0),
                     log_band_energies_ring_buf_.GetArrayView(1),
                     log_band_energies_ring_buf_.GetArrayView(2),
                     {band_corr_coeffs}, pitch_info_.period,
                     spectral_variability,
                     {feature_vector_.data(), feature_vector_.size()});
}

void RnnVadFeaturesExtractor::ComputeFftAndBandEnergyCoefficients(
    const size_t inverted_lag,
    rtc::ArrayView<std::complex<float>> fft_coeffs,
    rtc::ArrayView<float> band_energy_coeffs) {
  fft_.ForwardFft(
      half_band_buf_.GetBufferView(inverted_lag, kHalf20msFrameSize));
  fft_.CopyOutput(fft_coeffs);
  ComputeBandEnergies(fft_coeffs, {bands_indexes_}, band_energy_coeffs);
  // TODO(alessiob): Dump the FFT result for debugging to visually check that
  // the estimated pitch period is valid. It is ok to dump inverted lags with
  // which one can recover the FFT coefficients.
}

void RnnVadFeaturesExtractor::UpdatePitchEstimation() {
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
