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

#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/lp_residual.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

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

// Normalize and pack features into the final feature vector.
void WriteFeatureVector(
    const SpectralFeaturesExtractor* spectral_features_extractor,
    const int pitch_period_48kHz,
    rtc::ArrayView<float, kFeatureVectorSize> feature_vector) {
  // Add the spectral coefficients computed for the higher bands.
  spectral_features_extractor->CopySpectralCoefficients(
      {feature_vector.data() + kNumBandEnergyCoeffDeltas,
       kNumOpusBands - kNumBandEnergyCoeffDeltas},
      kNumBandEnergyCoeffDeltas);
  // Add average, first derivative and second derivative of the spectral
  // coefficients.
  spectral_features_extractor->ComputeAvgAndDeltas(
      {feature_vector.data(), kNumBandEnergyCoeffDeltas},
      {feature_vector.data() + kNumOpusBands, kNumBandEnergyCoeffDeltas},
      {feature_vector.data() + kNumOpusBands + kNumBandEnergyCoeffDeltas,
       kNumBandEnergyCoeffDeltas});
  size_t offset = kNumOpusBands + 2 * kNumBandEnergyCoeffDeltas;
  // Band correlation coefficients.
  spectral_features_extractor->ComputeCorrelation(
      {feature_vector.data() + offset, kNumBandCorrCoeffs});
  offset += kNumBandCorrCoeffs;
  // Pitch period (normalized).
  feature_vector[offset] = 0.01f * (pitch_period_48kHz - 300);
  offset++;
  // Spectral variability (normalized).
  feature_vector[offset] =
      spectral_features_extractor->ComputeSpectralVariability() /
          kSpectralCoeffsHistorySize -
      2.1f;
  offset++;
  RTC_DCHECK_EQ(kFeatureVectorSize, offset);
}

}  // namespace

RnnVadFeaturesExtractor::RnnVadFeaturesExtractor() : seq_buf_24kHz_(0.f) {
  Reset();
}

RnnVadFeaturesExtractor::~RnnVadFeaturesExtractor() = default;

void RnnVadFeaturesExtractor::Reset() {
  feature_vector_.fill(0.f);
}

rtc::ArrayView<const float, kFeatureVectorSize>
RnnVadFeaturesExtractor::GetFeatureVectorView() const {
  return {feature_vector_.data(), kFeatureVectorSize};
}

bool RnnVadFeaturesExtractor::ComputeFeaturesCheckSilence(
    rtc::ArrayView<const float, kFrameSize10ms24kHz> samples) {
  // Pre-processing.
  std::array<float, kFrameSize10ms24kHz> samples_filtered;
  rtc::ArrayView<float, kFrameSize10ms24kHz> samples_filtered_view(
      samples_filtered.data(), samples_filtered.size());
  hpf_.ProcessFrame(samples, {samples_filtered});
  // Feed buffers.
  seq_buf_24kHz_.Push(samples_filtered_view);
  // Extract the LP residual.
  std::array<float, kBufSize24kHz> lp_residual;
  {
    auto buf_view = seq_buf_24kHz_.GetBufferView();
    float lpc_coeffs[kNumLpcCoefficients];
    rtc::ArrayView<float, kNumLpcCoefficients> lpc_coeffs_view(
        lpc_coeffs, kNumLpcCoefficients);
    ComputeInverseFilterCoefficients(buf_view, lpc_coeffs_view);
    ComputeLpResidual(lpc_coeffs_view, buf_view,
                      {lp_residual.data(), lp_residual.size()});
  }
  // Search pitch on the LP-residual.
  pitch_info_48kHz_ =
      PitchSearch({lp_residual.data(), lp_residual.size()}, pitch_info_48kHz_);
  // Extract reference and lagged frames (according to the estimated pitch
  // period).
  auto reference_frame =
      seq_buf_24kHz_.GetBufferView(kPitchMaxPeriod24kHz, kFrameSize20ms24kHz);
  RTC_DCHECK_LE(pitch_info_48kHz_.period, 2 * kPitchMaxPeriod24kHz);
  auto lagged_frame = seq_buf_24kHz_.GetBufferView(
      kPitchMaxPeriod24kHz - pitch_info_48kHz_.period / 2, kFrameSize20ms24kHz);
  // Analyze reference and lagged frames checking if silence has been detected.
  if (spectral_features_extractor_.AnalyzeCheckSilence(
          {reference_frame.data(), kFrameSize20ms24kHz},
          {lagged_frame.data(), kFrameSize20ms24kHz})) {
    // Silence detected.
    return true;
  }
  // Finalize.
  WriteFeatureVector(&spectral_features_extractor_, pitch_info_48kHz_.period,
                     {feature_vector_.data(), feature_vector_.size()});
  return false;
}

}  // namespace rnn_vad
}  // namespace webrtc
