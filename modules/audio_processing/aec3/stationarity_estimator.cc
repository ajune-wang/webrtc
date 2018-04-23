/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/stationarity_estimator.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"

namespace webrtc {

namespace {
constexpr float kMinNoisePower = 10.f;
constexpr int kHangoverBlocks = kNumBlocksPerSecond / 20;

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

int StationarityEstimator::instance_count_ = 0;
constexpr float StationarityEstimator::kMinThr;
constexpr size_t StationarityEstimator::kMaxNumLookahead;

StationarityEstimator::StationarityEstimator()
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))) {
  Reset();
}

void StationarityEstimator::Reset() {
  noise_.Reset();
  hangover_long_window_.fill(0);
}

void StationarityEstimator::Update(rtc::ArrayView<const float> spectrum,
                                   size_t block_number,
                                   bool first_block) {
  if (buffer_.IsBlockNumberAlreadyUpdated(block_number) == false) {
    noise_.Update(spectrum);
    UpdateStationaryStatus(block_number, spectrum);
    data_dumper_->DumpRaw("aec3_stationarity_input_noise_spectrum", spectrum);
    data_dumper_->DumpRaw("blocks", block_number);
  }
  if (first_block) {
    data_dumper_->DumpRaw("aec3_stationarity_noise_spectrum",
                          noise_.Spectrum());
  }
}

void StationarityEstimator::UpdateStationaryStatus(
    int block_number,
    rtc::ArrayView<const float> spectrum) {
  int slot = buffer_.SetBlockNumberInSlot(block_number);
  for (size_t k = 0; k < all_ahead_stationary_.size(); ++k) {
    buffer_.SetElementProperties(spectrum[k], slot, k);
  }
}

bool StationarityEstimator::AreAllBandsStationary() {
  for (auto b : all_ahead_stationary_) {
    if (b == false)
      return false;
  }
  return true;
}

void StationarityEstimator::UpdateHangoverLongWindow() {
  bool reduce_hangover = AreAllBandsStationary();
  for (size_t k = 0; k < all_ahead_stationary_.size(); ++k) {
    if (all_ahead_stationary_[k] == false) {
      hangover_long_window_[k] = kHangoverBlocks;
    } else if (reduce_hangover) {
      hangover_long_window_[k] = std::max(hangover_long_window_[k] - 1, 0);
    }
  }
}

bool StationarityEstimator::IsBandStationaryLongWindow(
    const std::vector<size_t>& idx_lookahead,
    const std::vector<size_t>& idx_lookback,
    int k) const {
  float acumPower = 0.f;
  constexpr size_t framesLookAhead = kLongWindowSize;
  size_t slot;
  for (slot = 0; (slot < idx_lookahead.size()) && (slot <= framesLookAhead);
       ++slot) {
    acumPower += buffer_.GetPowerBand(idx_lookahead[slot], k);
  }
  RTC_DCHECK_EQ(slot + idx_lookback.size(), kLongWindowSize + 1);
  for (auto it = idx_lookback.cbegin(); it != idx_lookback.cend(); ++it) {
    acumPower += buffer_.GetPowerBand(*it, k);
  }
  float noise = (kLongWindowSize + 1) * GetStationarityPowerBand(k);
  bool stationary = ((acumPower / noise) < kMinThr);
  data_dumper_->DumpRaw("aec3_stationarity_long_ratio",
                        static_cast<float>(acumPower / noise));
  return stationary;
}

void StationarityEstimator::GetSlotsAheadBack(
    std::vector<size_t>* idx_lookahead,
    std::vector<size_t>* idx_lookback,
    size_t current_block_number,
    int num_ahead) {
  for (size_t block = 0; block < idx_lookahead->size(); ++block) {
    (*idx_lookahead)[block] =
        buffer_.GetSlotNumber(current_block_number + block);
  }
  size_t num_back;
  if (((size_t)num_ahead) >= kLongWindowSize) {
    num_back = 0;
  } else {
    num_back = kLongWindowSize - num_ahead;
  }
  if (current_block_number < num_back) {
    idx_lookback->resize(0);
  } else {
    for (size_t block = 0; block < num_back; ++block) {
      size_t block_number = current_block_number - block - 1;
      if (!buffer_.IsBlockNumberAlreadyUpdated(block_number)) {
        break;
      } else {
        idx_lookback->push_back((size_t)buffer_.GetSlotNumber(block_number));
      }
    }
  }
}

void StationarityEstimator::SmoothStationaryPerFreq() {
  std::array<bool, kFftLengthBy2Plus1> all_ahead_stationary_smooth;
  for (size_t k = 1; k < (kFftLengthBy2Plus1 - 1); ++k) {
    all_ahead_stationary_smooth[k] = all_ahead_stationary_[k - 1] &&
                                     all_ahead_stationary_[k] &&
                                     all_ahead_stationary_[k + 1];
  }

  all_ahead_stationary_smooth[0] = all_ahead_stationary_smooth[1];
  all_ahead_stationary_smooth[kFftLengthBy2Plus1 - 1] =
      all_ahead_stationary_smooth[kFftLengthBy2Plus1 - 2];

  all_ahead_stationary_ = all_ahead_stationary_smooth;
}

void StationarityEstimator::UpdateFlagsStationaryAhead(int current_block_number,
                                                       int num_lookahead) {
  std::vector<size_t> idx_lookahead(num_lookahead + 1);
  std::vector<size_t> idx_lookback(0);
  GetSlotsAheadBack(&idx_lookahead, &idx_lookback, current_block_number,
                    num_lookahead);

  all_ahead_stationary_.fill(true);
  for (size_t k = 0; k < all_ahead_stationary_.size(); ++k) {
    all_ahead_stationary_[k] =
        IsBandStationaryLongWindow(idx_lookahead, idx_lookback, k);
    data_dumper_->DumpRaw("aec3_stationarity_ahead",
                          static_cast<int>(all_ahead_stationary_[k]));
  }
  UpdateHangoverLongWindow();
  SmoothStationaryPerFreq();

  for (size_t k = 0; k < all_ahead_stationary_.size(); k++) {
    data_dumper_->DumpRaw("aec3_stationarity_hangover_long",
                          static_cast<int>(IsBandStationaryAhead(k)));
  }
}

StationarityEstimator::NoiseSpectrum::NoiseSpectrum() {
  Reset();
}

StationarityEstimator::NoiseSpectrum::~NoiseSpectrum() = default;

void StationarityEstimator::NoiseSpectrum::Reset() {
  power_ = 0.f;
  block_counter_ = 0;
  noise_spectrum_.fill(kMinNoisePower);
}

constexpr float StationarityEstimator::NoiseSpectrum::kAlpha;
constexpr float StationarityEstimator::NoiseSpectrum::kAlphaInit;
constexpr float StationarityEstimator::NoiseSpectrum::kNBlocksInitialPhase;
constexpr float StationarityEstimator::NoiseSpectrum::kTiltAlpha;

float StationarityEstimator::NoiseSpectrum::GetAlpha() const {
  if (block_counter_ > (kNBlocksInitialPhase + kNBlocksAverageInitPhase)) {
    return kAlpha;
  } else {
    return kAlphaInit -
           kTiltAlpha * (block_counter_ - kNBlocksAverageInitPhase);
  }
}

float StationarityEstimator::NoiseSpectrum::UpdateBandBySmoothing(
    float power_band,
    float power_band_noise,
    float alpha) const {
  float power_band_noise_updated = power_band_noise;
  if (power_band_noise < power_band) {
    float alpha_inc = alpha * (power_band_noise / power_band);
    if (block_counter_ > kNBlocksInitialPhase) {
      if (10.f * power_band_noise < power_band) {
        alpha_inc = alpha_inc * 0.1f;
      }
    }
    power_band_noise_updated += alpha_inc * (power_band - power_band_noise);
  } else {
    power_band_noise_updated += alpha * (power_band - power_band_noise);
    power_band_noise_updated =
        std::max(power_band_noise_updated, kMinNoisePower);
  }
  return power_band_noise_updated;
}

void StationarityEstimator::NoiseSpectrum::Update(
    rtc::ArrayView<const float> spectrum) {
  float alpha;
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, spectrum.size());
  ++block_counter_;

  alpha = GetAlpha();
  for (size_t k = 0; k < spectrum.size(); ++k) {
    if (block_counter_ <= kNBlocksAverageInitPhase) {
      noise_spectrum_[k] += (1 / kNBlocksAverageInitPhase) * spectrum[k];
    } else {
      noise_spectrum_[k] =
          UpdateBandBySmoothing(spectrum[k], noise_spectrum_[k], alpha);
    }
  }
  power_ = SpectralPower(noise_spectrum_);
}

float StationarityEstimator::NoiseSpectrum::PowerBand(size_t band) const {
  RTC_DCHECK_LT(band, noise_spectrum_.size());
  return noise_spectrum_[band];
}

StationarityEstimator::CircularBuffer::Element::Element() : block_number_(-1) {}

StationarityEstimator::CircularBuffer::CircularBuffer() = default;

bool StationarityEstimator::CircularBuffer::IsBlockNumberAlreadyUpdated(
    int block_number) const {
  int slot_number = GetSlotNumber(block_number);
  return (slots_[slot_number].GetBlockNumber() == block_number);
}

int StationarityEstimator::CircularBuffer::SetBlockNumberInSlot(
    int block_number) {
  int slot = GetSlotNumber(block_number);
  slots_[slot].SetBlockNumber(block_number);
  return slot;
}

}  // namespace webrtc
