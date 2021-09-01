/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/spectral_features.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_compare.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr float kSilenceThreshold = 0.04f;

// Computes the new cepstral difference stats and pushes them into the passed
// symmetric matrix buffer.
void UpdateCepstralDifferenceStats(
    rtc::ArrayView<const float, kNumBands> new_cepstral_coeffs,
    const RingBuffer<float, kNumBands, kCepstralCoeffsHistorySize>& ring_buf,
    SymmetricMatrixBuffer<float, kCepstralCoeffsHistorySize>* sym_matrix_buf) {
  RTC_DCHECK(sym_matrix_buf);
  // Compute the new cepstral distance stats.
  std::array<float, kCepstralCoeffsHistorySize - 1> distances;
  for (int i = 0; i < kCepstralCoeffsHistorySize - 1; ++i) {
    const int delay = i + 1;
    auto old_cepstral_coeffs = ring_buf.GetArrayView(delay);
    distances[i] = 0.f;
    for (int k = 0; k < kNumBands; ++k) {
      const float c = new_cepstral_coeffs[k] - old_cepstral_coeffs[k];
      distances[i] += c * c;
    }
  }
  // Push the new spectral distance stats into the symmetric matrix buffer.
  sym_matrix_buf->Push(distances);
}

// First half of the Vorbis window used in `ComputeWindowedForwardFft()`
// computed in `spectral_features_unittest.cc`.
constexpr float kVorbisHalfWindow[]{
    0.000000035046f, 0.000000315402f, 0.000000876067f, 0.000001716944f,
    0.000002837888f, 0.000004238707f, 0.000005919159f, 0.000007878954f,
    0.000010117751f, 0.000012635160f, 0.000015430740f, 0.000018504001f,
    0.000021854397f, 0.000025481335f, 0.000029384164f, 0.000033562181f,
    0.000038014630f, 0.000042740696f, 0.000047739508f, 0.000053010142f,
    0.000058551606f, 0.000064362859f, 0.000070442788f, 0.000076790224f,
    0.000083403931f, 0.000090282621f, 0.000097424920f, 0.000104829400f,
    0.000112494570f, 0.000120418845f, 0.000128600601f, 0.000137038121f,
    0.000145729631f, 0.000154673253f, 0.000163867051f, 0.000173309032f,
    0.000182997086f, 0.000192929059f, 0.000203102696f, 0.000213515654f,
    0.000224165531f, 0.000235049811f, 0.000246165931f, 0.000257511187f,
    0.000269082870f, 0.000280878070f, 0.000292893936f, 0.000305127382f,
    0.000317575294f, 0.000330234529f, 0.000343101739f, 0.000356173550f,
    0.000369446498f, 0.000382917031f, 0.000396581483f, 0.000410436071f,
    0.000424476981f, 0.000438700285f, 0.000453101937f, 0.000467677863f,
    0.000482423842f, 0.000497335568f, 0.000512408675f, 0.000527638709f,
    0.000543021073f, 0.000558551168f, 0.000574224279f, 0.000590035517f,
    0.000605980109f, 0.000622053049f, 0.000638249330f, 0.000654563715f,
    0.000670991198f, 0.000687526364f, 0.000704163918f, 0.000720898504f,
    0.000737724709f, 0.000754636887f, 0.000771629624f, 0.000788697158f,
    0.000805833843f, 0.000823033974f, 0.000840291788f, 0.000857601466f,
    0.000874957128f, 0.000892352895f, 0.000909782888f, 0.000927241112f,
    0.000944721687f, 0.000962218561f, 0.000979725737f, 0.000997237279f,
    0.001014747191f, 0.001032249304f, 0.001049737795f, 0.001067206496f,
    0.001084649586f, 0.001102061011f, 0.001119434834f, 0.001136765117f,
    0.001154046040f, 0.001171271666f, 0.001188436290f, 0.001205533976f,
    0.001222559134f, 0.001239506062f, 0.001256369171f, 0.001273142989f,
    0.001289821812f, 0.001306400518f, 0.001322873635f, 0.001339235925f,
    0.001355482265f, 0.001371607650f, 0.001387606957f, 0.001403475530f,
    0.001419208362f, 0.001434801030f, 0.001450248761f, 0.001465547248f,
    0.001480692183f, 0.001495679375f, 0.001510504633f, 0.001525164233f,
    0.001539654098f, 0.001553970738f, 0.001568110543f, 0.001582070137f,
    0.001595846261f, 0.001609435771f, 0.001622835756f, 0.001636043307f,
    0.001649055863f, 0.001661870861f, 0.001674485859f, 0.001686898759f,
    0.001699107466f, 0.001711110002f, 0.001722904737f, 0.001734490041f,
    0.001745864633f, 0.001757026999f, 0.001767976210f, 0.001778711216f,
    0.001789231203f, 0.001799535705f, 0.001809624140f, 0.001819496159f,
    0.001829151646f, 0.001838590601f, 0.001847813255f, 0.001856819610f,
    0.001865610364f, 0.001874186099f, 0.001882547396f, 0.001890695188f,
    0.001898630522f, 0.001906354679f, 0.001913868706f, 0.001921174116f,
    0.001928272541f, 0.001935165608f, 0.001941855182f, 0.001948343008f,
    0.001954631414f, 0.001960722264f, 0.001966617769f, 0.001972320722f,
    0.001977833221f, 0.001983158058f, 0.001988297794f, 0.001993255224f,
    0.001998033142f, 0.002002634341f, 0.002007062314f, 0.002011319622f,
    0.002015409525f, 0.002019335516f, 0.002023100387f, 0.002026708098f,
    0.002030161442f, 0.002033464145f, 0.002036619931f, 0.002039631829f,
    0.002042503562f, 0.002045238623f, 0.002047840971f, 0.002050314099f,
    0.002052661264f, 0.002054886660f, 0.002056993777f, 0.002058986109f,
    0.002060867613f, 0.002062641783f, 0.002064312343f, 0.002065883018f,
    0.002067357302f, 0.002068738919f, 0.002070031362f, 0.002071238356f,
    0.002072363161f, 0.002073409734f, 0.002074381104f, 0.002075280761f,
    0.002076112200f, 0.002076878911f, 0.002077583689f, 0.002078230260f,
    0.002078821417f, 0.002079360420f, 0.002079850296f, 0.002080294071f,
    0.002080694307f, 0.002081054263f, 0.002081376268f, 0.002081663348f,
    0.002081917832f, 0.002082142280f, 0.002082339022f, 0.002082510386f,
    0.002082658932f, 0.002082786290f, 0.002082895022f, 0.002082986524f,
    0.002083063126f, 0.002083126223f, 0.002083177678f, 0.002083218889f,
    0.002083251253f, 0.002083276398f, 0.002083295025f, 0.002083308762f,
    0.002083318541f, 0.002083325060f, 0.002083329018f, 0.002083331579f,
    0.002083332743f, 0.002083333209f, 0.002083333442f, 0.002083333442f};

// Writes a windowed version of `frame` into `fft_input_buffer` and computes the
// forward FFT. Writes the output into `fft_output_buffer`; the Fourier
// coefficient corresponding to the Nyquist frequency is set to zero.
void ComputeWindowedForwardFft(
    rtc::ArrayView<const float, kFrameSize20ms24kHz> frame,
    Pffft::FloatBuffer& fft_input_buffer,
    Pffft::FloatBuffer& fft_output_buffer,
    Pffft& fft) {
  constexpr int kHalfSize = arraysize(kVorbisHalfWindow);
  RTC_DCHECK_EQ(frame.size(), 2 * kHalfSize);
  // Apply windowing.
  auto in = fft_input_buffer.GetView();
  for (int i = 0, j = kFrameSize20ms24kHz - 1; i < kHalfSize; ++i, --j) {
    in[i] = frame[i] * kVorbisHalfWindow[i];
    in[j] = frame[j] * kVorbisHalfWindow[i];
  }
  fft.ForwardTransform(fft_input_buffer, &fft_output_buffer, /*ordered=*/true);
  // Set the Nyquist frequency coefficient to zero.
  fft_output_buffer.GetView()[1] = 0.0f;
}

void ComputeSpectralAutoCorrelation(
    rtc::ArrayView<const float> x,
    rtc::ArrayView<float, kOpusBands24kHz> auto_correlation) {
  ComputeSpectralCrossCorrelation(x, x, auto_correlation);
}

}  // namespace

SpectralFeaturesExtractor::SpectralFeaturesExtractor()
    : fft_(kFrameSize20ms24kHz, Pffft::FftType::kReal),
      fft_buffer_(fft_.CreateBuffer()),
      reference_frame_fft_(fft_.CreateBuffer()),
      lagged_frame_fft_(fft_.CreateBuffer()) {}

SpectralFeaturesExtractor::~SpectralFeaturesExtractor() = default;

void SpectralFeaturesExtractor::Reset() {
  cepstral_coeffs_ring_buf_.Reset();
  cepstral_diffs_buf_.Reset();
}

bool SpectralFeaturesExtractor::CheckSilenceComputeFeatures(
    rtc::ArrayView<const float, kFrameSize20ms24kHz> reference_frame,
    rtc::ArrayView<const float, kFrameSize20ms24kHz> lagged_frame,
    rtc::ArrayView<float, kNumBands - kNumLowerBands> higher_bands_cepstrum,
    rtc::ArrayView<float, kNumLowerBands> average,
    rtc::ArrayView<float, kNumLowerBands> first_derivative,
    rtc::ArrayView<float, kNumLowerBands> second_derivative,
    rtc::ArrayView<float, kNumLowerBands> bands_cross_corr,
    float* variability) {
  // Compute the Opus band energies for the reference frame.
  ComputeWindowedForwardFft(reference_frame, *fft_buffer_,
                            *reference_frame_fft_, fft_);
  ComputeSpectralAutoCorrelation(reference_frame_fft_->GetConstView(),
                                 reference_frame_bands_energy_);
  // Check if the reference frame has silence.
  const float tot_energy =
      std::accumulate(reference_frame_bands_energy_.begin(),
                      reference_frame_bands_energy_.end(), 0.0f);
  if (tot_energy < kSilenceThreshold) {
    return true;
  }
  // Compute the Opus band energies for the lagged frame.
  ComputeWindowedForwardFft(lagged_frame, *fft_buffer_, *lagged_frame_fft_,
                            fft_);
  ComputeSpectralAutoCorrelation(lagged_frame_fft_->GetConstView(),
                                 lagged_frame_bands_energy_);
  // Log of the band energies for the reference frame.
  std::array<float, kNumBands> log_bands_energy;
  ComputeSmoothedLogMagnitudeSpectrum(reference_frame_bands_energy_,
                                      log_bands_energy);
  // Reference frame cepstrum.
  std::array<float, kNumBands> cepstrum;
  ComputeDct(log_bands_energy, cepstrum);
  // Ad-hoc correction terms for the first two cepstral coefficients.
  cepstrum[0] -= 12.0f;
  cepstrum[1] -= 4.0f;
  // Update the ring buffer and the cepstral difference stats.
  cepstral_coeffs_ring_buf_.Push(cepstrum);
  UpdateCepstralDifferenceStats(cepstrum, cepstral_coeffs_ring_buf_,
                                &cepstral_diffs_buf_);
  // Write the higher bands cepstral coefficients.
  RTC_DCHECK_EQ(cepstrum.size() - kNumLowerBands, higher_bands_cepstrum.size());
  std::copy(cepstrum.begin() + kNumLowerBands, cepstrum.end(),
            higher_bands_cepstrum.begin());
  // Compute and write remaining features.
  ComputeAvgAndDerivatives(average, first_derivative, second_derivative);
  ComputeNormalizedCepstralCorrelation(bands_cross_corr);
  RTC_DCHECK(variability);
  *variability = ComputeVariability();
  return false;
}

void SpectralFeaturesExtractor::ComputeAvgAndDerivatives(
    rtc::ArrayView<float, kNumLowerBands> average,
    rtc::ArrayView<float, kNumLowerBands> first_derivative,
    rtc::ArrayView<float, kNumLowerBands> second_derivative) const {
  auto curr = cepstral_coeffs_ring_buf_.GetArrayView(0);
  auto prev1 = cepstral_coeffs_ring_buf_.GetArrayView(1);
  auto prev2 = cepstral_coeffs_ring_buf_.GetArrayView(2);
  RTC_DCHECK_EQ(average.size(), first_derivative.size());
  RTC_DCHECK_EQ(first_derivative.size(), second_derivative.size());
  RTC_DCHECK_LE(average.size(), curr.size());
  for (int i = 0; rtc::SafeLt(i, average.size()); ++i) {
    // Average, kernel: [1, 1, 1].
    average[i] = curr[i] + prev1[i] + prev2[i];
    // First derivative, kernel: [1, 0, - 1].
    first_derivative[i] = curr[i] - prev2[i];
    // Second derivative, Laplacian kernel: [1, -2, 1].
    second_derivative[i] = curr[i] - 2 * prev1[i] + prev2[i];
  }
}

void SpectralFeaturesExtractor::ComputeNormalizedCepstralCorrelation(
    rtc::ArrayView<float, kNumLowerBands> bands_cross_corr) {
  ComputeSpectralCrossCorrelation(reference_frame_fft_->GetConstView(),
                                  lagged_frame_fft_->GetConstView(),
                                  bands_cross_corr_);
  // Normalize.
  for (int i = 0; rtc::SafeLt(i, bands_cross_corr_.size()); ++i) {
    bands_cross_corr_[i] =
        bands_cross_corr_[i] /
        std::sqrt(0.001f + reference_frame_bands_energy_[i] *
                               lagged_frame_bands_energy_[i]);
  }
  // Cepstrum.
  ComputeDct(bands_cross_corr_, bands_cross_corr);
  // Ad-hoc correction terms for the first two cepstral coefficients.
  bands_cross_corr[0] -= 1.3f;
  bands_cross_corr[1] -= 0.9f;
}

float SpectralFeaturesExtractor::ComputeVariability() const {
  // Compute cepstral variability score.
  float variability = 0.f;
  for (int delay1 = 0; delay1 < kCepstralCoeffsHistorySize; ++delay1) {
    float min_dist = std::numeric_limits<float>::max();
    for (int delay2 = 0; delay2 < kCepstralCoeffsHistorySize; ++delay2) {
      if (delay1 == delay2)  // The distance would be 0.
        continue;
      min_dist =
          std::min(min_dist, cepstral_diffs_buf_.GetValue(delay1, delay2));
    }
    variability += min_dist;
  }
  // Normalize (based on training set stats).
  // TODO(bugs.webrtc.org/10480): Isolate normalization from feature extraction.
  return variability / kCepstralCoeffsHistorySize - 2.1f;
}

}  // namespace rnn_vad
}  // namespace webrtc
