/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_audibility.h"

#include <math.h>

#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"

namespace webrtc {
namespace {

constexpr float kMinNoisePower = 100.f;
constexpr size_t kMaxNumLookahead = 10;

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

bool IsStationary(rtc::ArrayView<const float> signal_spectrum,
                  rtc::ArrayView<const float> noise_spectrum) {
  int stationary_bands = 0;
  int nonstationary_bands = 0;

  // Detect stationary and highly nonstationary bands.
  for (size_t k = 1; k < 40; k++) {
    if (signal_spectrum[k] < 6 * noise_spectrum[k]) {
      ++stationary_bands;
    } else if (signal_spectrum[k] > 9 * noise_spectrum[k]) {
      ++nonstationary_bands;
    }
  }

  // Use the detected number of bands to classify the overall signal
  // stationarity.
  return stationary_bands > 15 ||
         (stationary_bands > 9 && nonstationary_bands < 2);
}

}  // namespace

int EchoAudibility::instance_count_ = 0;

EchoAudibility::EchoAudibility()
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      inaudible_blocks_(kMaxNumLookahead + 1, true) {}
EchoAudibility::~EchoAudibility() = default;

void EchoAudibility::Update(const RenderBuffer& render_buffer,
                            size_t delay_blocks,
                            const std::array<float, kBlockSize>& s) {
  size_t num_lookahead = render_buffer.Headroom() - delay_blocks;

  inaudible_blocks_.resize(std::min(kMaxNumLookahead + 1, num_lookahead + 1));
  inaudible_blocks_[0] =
      stationarity_.Update(render_buffer.Spectrum(delay_blocks));

  if (++convergence_counter_ < 20) {
    std::fill(inaudible_blocks_.begin(), inaudible_blocks_.end(), false);
  } else {
    for (size_t k = 1; k < inaudible_blocks_.size(); k++) {
      inaudible_blocks_[k] =
          stationarity_.Analyze(render_buffer.Spectrum(delay_blocks + k));
    }
  }

  auto& x = render_buffer.Block(-delay_blocks)[0];
  auto result_x = std::minmax_element(x.begin(), x.end());
  const float x_abs = std::max(fabsf(*result_x.first), fabsf(*result_x.second));
  low_farend_counter_ = x_abs < 100.f ? low_farend_counter_ + 1 : 0;

  if (++convergence_counter_ < 20 && !inaudible_blocks_[0]) {
    auto result_s = std::minmax_element(s.begin(), s.end());
    const float s_abs =
        std::max(fabsf(*result_s.first), fabsf(*result_s.second));
    inaudible_blocks_[0] = (s_abs < 30.f) || low_farend_counter_ > 20;
  }

  num_nonaudible_blocks_ = 0;
  while (num_nonaudible_blocks_ < inaudible_blocks_.size() &&
         inaudible_blocks_[num_nonaudible_blocks_]) {
    ++num_nonaudible_blocks_;
  }
  if (num_nonaudible_blocks_ > 0)
    printf("A:%zu\n", num_nonaudible_blocks_);

  float speech = SpectralPower(render_buffer.Spectrum(delay_blocks));
  float noise = std::min(stationarity_.StationaryPower(), 800.f * 800.f);
  noise = std::max(noise, 30.f * 30.f);

  const float min = noise * 4.f;
  const float max = noise * 1000.f;

  if (max == 0.f) {
    residual_echo_scaling_ = 1.f;
  } else {
    residual_echo_scaling_ = speech / max;
    residual_echo_scaling_ = std::min(1.f, residual_echo_scaling_);
    if (residual_echo_scaling_ < min / max * min / max * min / max) {
      residual_echo_scaling_ = 0.f;
    }
    residual_echo_scaling_ = std::max(0.f, residual_echo_scaling_);
  }

  data_dumper_->DumpRaw("aec3_render_stationary_power",
                        stationarity_.StationaryPower());
  data_dumper_->DumpRaw("aec3_num_non_audible_echo_blocks",
                        num_nonaudible_blocks_);
  data_dumper_->DumpRaw("aec3_residual_echo_scaling", residual_echo_scaling_);
}

EchoAudibility::Stationarity::Stationarity() = default;

EchoAudibility::Stationarity::~Stationarity() = default;

bool EchoAudibility::Stationarity::Update(
    rtc::ArrayView<const float> spectrum) {
  ++block_counter_;

  // Update the noise spectrum based on the signal spectrum.
  noise_.Update(spectrum, block_counter_ == 1);

  bool stationarity = IsStationary(spectrum, noise_.Spectrum());

  stationarity_counter =
      stationarity_ == stationarity ? stationarity_counter + 1 : 0;
  stationarity_ = stationarity;

  return stationarity_counter >= 3 ? stationarity_ : false;
}

bool EchoAudibility::Stationarity::Analyze(
    rtc::ArrayView<const float> spectrum) const {
  if (stationarity_counter < 3) {
    return false;
  }
  return IsStationary(spectrum, noise_.Spectrum());
}

int EchoAudibility::Stationarity::NoiseSpectrum::instance_count_ = 0;

EchoAudibility::Stationarity::NoiseSpectrum::NoiseSpectrum()
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      power_(0) {
  noise_spectrum_.fill(kMinNoisePower);
  counters_.fill(0);
}

EchoAudibility::Stationarity::NoiseSpectrum::~NoiseSpectrum() = default;

void EchoAudibility::Stationarity::NoiseSpectrum::Update(
    rtc::ArrayView<const float> spectrum,
    bool first_update) {
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, spectrum.size());

  if (first_update) {
    std::copy(spectrum.begin(), spectrum.end(), noise_spectrum_.begin());

    for (auto& v : noise_spectrum_) {
      v = std::max(v, kMinNoisePower);
    }
  } else {
    for (size_t k = 0; k < spectrum.size(); ++k) {
      float new_noise =
          noise_spectrum_[k] + 0.05f * (spectrum[k] - noise_spectrum_[k]);
      if (noise_spectrum_[k] < spectrum[k]) {
        if (++counters_[k] > kNumBlocksPerSecond) {
          noise_spectrum_[k] = std::min(1.01f * noise_spectrum_[k], new_noise);
        }
      } else {
        noise_spectrum_[k] = std::max(0.99f * noise_spectrum_[k], new_noise);
        noise_spectrum_[k] = std::max(noise_spectrum_[k], kMinNoisePower);
        counters_[k] = 0;
      }
    }
  }

  power_ = SpectralPower(noise_spectrum_);
  data_dumper_->DumpRaw("aec3_audibility_noisy_spectrum", noise_spectrum_);
}

}  // namespace webrtc
