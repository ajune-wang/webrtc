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

// Get even samples from |src| and push them into |dst|.
void PushDecimated2xSamples(
    rtc::ArrayView<const float, kFrameSize10ms24kHz> src,
    SequenceBuffer<float, kBufSize12kHz, kFrameSize10ms12kHz>* dst) {
  RTC_DCHECK(dst);
  // Copy even samples from |src|.
  std::array<float, kFrameSize10ms12kHz> samples_decimated;
  for (size_t i = 0; i < samples_decimated.size(); ++i)
    samples_decimated[i] = src[2 * i];
  // Push decimated samples into |dst|.
  dst->Push({samples_decimated.data(), samples_decimated.size()});
}

// Normalize and pack features into the final feature vector.
void WriteFeatureVector(
    const SpectralFeaturesExtractor* spectral_features_extractor,
    const int pitch_period_48kHz,
    rtc::ArrayView<float, kFeatureVectorSize> feature_vector) {
  size_t offset = 0;
  // Average spectral coefficients.
  spectral_features_extractor->ComputeAverage(
      {feature_vector.data() + offset, kNumOpusBands});
  feature_vector[offset] -= 12;
  feature_vector[offset + 1] -= 4;
  offset += kNumOpusBands;
  // Spectral coefficients (first derivative).
  spectral_features_extractor->ComputeDelta1(
      {feature_vector.data() + offset, kNumBandEnergyCoeffDeltas});
  offset += kNumBandEnergyCoeffDeltas;
  // Spectral coefficients (second derivative).
  spectral_features_extractor->ComputeDelta2(
      {feature_vector.data() + offset, kNumBandEnergyCoeffDeltas});
  offset += kNumBandEnergyCoeffDeltas;
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
          kBandEnergyCoeffsHistorySize -
      2.1f;
  offset++;
  RTC_DCHECK_EQ(kFeatureVectorSize, offset);
}

}  // namespace

RnnVadFeaturesExtractor::RnnVadFeaturesExtractor()
    : seq_buf_24kHz_(0.f), seq_buf_12kHz_(0.f) {
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
  PushDecimated2xSamples(samples_filtered_view, &seq_buf_12kHz_);
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
      seq_buf_12kHz_.GetBufferView(kPitchMaxPeriod12kHz, kFrameSize20ms12kHz);
  auto lagged_frame = seq_buf_12kHz_.GetBufferView(
      kPitchMaxPeriod12kHz - pitch_info_48kHz_.period / 4, kFrameSize20ms12kHz);
  // Analyze reference and lagged frames checking if silence has been detected.
  if (spectral_features_extractor_.AnalyzeCheckSilence(reference_frame,
                                                       lagged_frame)) {
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
