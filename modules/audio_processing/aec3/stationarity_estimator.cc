/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/stationarity_estimator.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"

namespace webrtc {

namespace {
constexpr float kMinNoisePower = 10.f;
constexpr float alpha_spectrum = 0.086899f;

bool IsStationary(std::array<float, kFftLengthBy2Plus1> signal_spectrum,
                  rtc::ArrayView<const float> noise_spectrum) {
  int stationary_bands = 0;
  int nonstationary_bands = 0;

  // Detect stationary and highly nonstationary bands.
  for (size_t k = 1; k < 40; k++) {
    if (signal_spectrum[k] < 10 * noise_spectrum[k]) {
      ++stationary_bands;
    } else {
      ++nonstationary_bands;
    }
  }

  // Use the detected number of bands to classify the overall signal
  // stationarity.
  return stationary_bands > 15 ||
         (stationary_bands > 9 && nonstationary_bands < 2);
}

float SpectralPower(rtc::ArrayView<const float> spectrum) {
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, spectrum.size());
  float power = 0;
  for (size_t k = 1; k < spectrum.size() - 1; ++k) {
    power += spectrum[k];
  }
  power *= 2;
  power += spectrum[0] + spectrum[spectrum.size() - 1];
  constexpr float kOneByFftLengthSqr = 1.f / kFftLength * 1.f / kFftLength;
  power *= kOneByFftLengthSqr;
  return power;
}

}  // namespace

StationarityEstimator::~StationarityEstimator() = default;

bool StationarityEstimator::IsFrameStationary() {
  return IsStationary(smooth_spectrum_, noise_.Spectrum());
}

int StationarityEstimator::instance_count_ = 0;

StationarityEstimator::StationarityEstimator()
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))) {
  smooth_spectrum_.fill(0.f);
  stationary_.fill(false);
}

bool StationarityEstimator::Update(rtc::ArrayView<const float> spectrum) {
  // Update the noise spectrum based on the signal spectrum.
  noise_.Update(spectrum);

  ComputeSmoothSpectrum(spectrum);

  UpdateStationaryStatus();

  bool stationary_frame = IsStationary(smooth_spectrum_, noise_.Spectrum());

  stationarity_counter =
      stationary_frame_ == stationary_frame ? stationarity_counter + 1 : 0;
  stationary_frame_ = stationary_frame;

  data_dumper_->DumpRaw("aec3_stationarity_noise_spectrum", noise_.Spectrum());
  data_dumper_->DumpRaw("aec3_stationarity_smooth_spectrum", smooth_spectrum_);
  return stationarity_counter >= 3 ? stationary_frame_ : false;
}

void StationarityEstimator::UpdateStationaryStatus() {
  for (size_t k = 0; k < smooth_spectrum_.size(); ++k) {
    float noise = GetStationarityPowerBand(k);
    float frame_power = GetPowerBandSmthSpectrum(k);
    stationary_[k] = frame_power < 10 * noise;
  }
}

bool StationarityEstimator::IsBandStationary(size_t k) const {
  return stationary_[k];
}

void StationarityEstimator::ComputeSmoothSpectrum(
    rtc::ArrayView<const float> spectrum) {
  RTC_DCHECK_EQ(spectrum.size(), kFftLengthBy2Plus1);
  for (size_t k = 0; k < spectrum.size(); ++k) {
    smooth_spectrum_[k] += alpha_spectrum * (spectrum[k] - smooth_spectrum_[k]);
  }
}

bool StationarityEstimator::Analyze(
    rtc::ArrayView<const float> spectrum) const {
  if (stationarity_counter < 3) {
    return false;
  }
  return false;  // JVPdelme
  //  return IsStationary(spectrum, noise_.Spectrum());
}

StationarityEstimator::NoiseSpectrum::NoiseSpectrum() : power_(0) {
  noise_spectrum_.fill(kMinNoisePower);
}

StationarityEstimator::NoiseSpectrum::~NoiseSpectrum() = default;

constexpr float StationarityEstimator::NoiseSpectrum::alpha;
constexpr float StationarityEstimator::NoiseSpectrum::alpha_ini;
constexpr float StationarityEstimator::NoiseSpectrum::nBlocksInitialPhase;
constexpr float StationarityEstimator::NoiseSpectrum::tilt_alpha;

float StationarityEstimator::NoiseSpectrum::GetAlpha() const {
  if (block_counter_ > nBlocksInitialPhase) {
    return alpha;
  } else {
    return alpha_ini - tilt_alpha * block_counter_;
  }
}

void StationarityEstimator::NoiseSpectrum::Update(
    rtc::ArrayView<const float> spectrum) {
  float alpha;
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, spectrum.size());
  ++block_counter_;

  alpha = GetAlpha();
  for (size_t k = 0; k < spectrum.size(); ++k) {
    if (noise_spectrum_[k] < spectrum[k]) {
      float alpha_inc;

      if (block_counter_ > nBlocksInitialPhase) {
        alpha_inc = alpha * noise_spectrum_[k] / spectrum[k];
        if (noise_spectrum_[k] < 100.f * spectrum[k]) {
          alpha_inc = alpha_inc / 10.f;
        }
      } else {
        alpha_inc = alpha;
      }
      float new_noise =
          noise_spectrum_[k] + alpha_inc * (spectrum[k] - noise_spectrum_[k]);
      noise_spectrum_[k] = std::min(1.01f * noise_spectrum_[k], new_noise);

    } else {
      float new_noise =
          noise_spectrum_[k] + alpha * (spectrum[k] - noise_spectrum_[k]);
      noise_spectrum_[k] = std::max(0.99f * noise_spectrum_[k], new_noise);
      noise_spectrum_[k] = std::max(noise_spectrum_[k], kMinNoisePower);
    }
  }

  power_ = SpectralPower(noise_spectrum_);
}

float StationarityEstimator::NoiseSpectrum::PowerBand(size_t band) const {
  RTC_DCHECK_LT(band, noise_spectrum_.size());
  return noise_spectrum_[band];
}

}  // namespace webrtc
