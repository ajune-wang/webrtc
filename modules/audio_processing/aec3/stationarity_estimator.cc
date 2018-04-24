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

#include <algorithm>
#include <array>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"

namespace webrtc {

namespace {
constexpr float kMinNoisePower = 10.f;
constexpr int kHangoverBlocks = kNumBlocksPerSecond / 20;
constexpr float kAlpha = 0.004f;
constexpr float kAlphaInit = 0.04f;
constexpr float kNBlocksInitialPhase = kNumBlocksPerSecond * 2.;
constexpr float kTiltAlpha = (kAlphaInit - kAlpha) / kNBlocksInitialPhase;
constexpr float kNBlocksAverageInitPhase = 20;

constexpr size_t kLongWindowSize = 13;
}  // namespace

StationarityEstimator::~StationarityEstimator() = default;

int StationarityEstimator::instance_count_ = 0;

constexpr size_t StationarityEstimator::kMaxNumLookahead;

StationarityEstimator::StationarityEstimator()
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))) {
  static_assert(StationarityEstimator::CircularBuffer::kCircularBufferSize >=
                    (kLongWindowSize + 1),
                "Mismatch between the window size and the buffer size.");
  Reset();
}

void StationarityEstimator::Reset() {
  noise_.Reset();
  hangover_.fill(0);
  stationary_flag_.fill(false);
}

void StationarityEstimator::Update(rtc::ArrayView<const float> spectrum,
                                   size_t block_number,
                                   bool first_block) {
  if (buffer_.IsBlockNumberAlreadyUpdated(block_number) == false) {
    noise_.Update(spectrum);
    WriteInfoFrameInSlot(block_number, spectrum);
    data_dumper_->DumpRaw("aec3_stationarity_input_noise_spectrum", spectrum);
    data_dumper_->DumpRaw("blocks", block_number);
  }
  if (first_block) {
    data_dumper_->DumpRaw("aec3_stationarity_noise_spectrum",
                          noise_.Spectrum());
  }
}

void StationarityEstimator::WriteInfoFrameInSlot(
    size_t block_number,
    rtc::ArrayView<const float> spectrum) {
  size_t slot = buffer_.SetBlockNumberInSlot(block_number);
  for (size_t k = 0; k < spectrum.size(); ++k) {
    buffer_.SetElementProperties(spectrum[k], slot, k);
  }
}

void StationarityEstimator::UpdateFlagsStationaryAhead(
    size_t current_block_number,
    size_t num_lookahead) {
  std::vector<size_t> idx_lookahead(
      std::min(num_lookahead + 1, kLongWindowSize));
  std::vector<size_t> idx_lookback(0);
  GetSlotsAheadBack(&idx_lookahead, &idx_lookback, current_block_number);

  for (size_t k = 0; k < stationary_flag_.size(); ++k) {
    stationary_flag_[k] =
        EstimateBandStationarity(idx_lookahead, idx_lookback, k);
    data_dumper_->DumpRaw("aec3_stationarity_ahead",
                          static_cast<int>(stationary_flag_[k]));
  }
  UpdateHangover();
  SmoothStationaryPerFreq();

  for (size_t k = 0; k < stationary_flag_.size(); k++) {
    data_dumper_->DumpRaw("aec3_stationarity_hangover_long",
                          static_cast<int>(IsBandStationaryAhead(k)));
  }
}

bool StationarityEstimator::AreAllBandsStationary() {
  for (auto b : stationary_flag_) {
    if (b == false)
      return false;
  }
  return true;
}

void StationarityEstimator::UpdateHangover() {
  bool reduce_hangover = AreAllBandsStationary();
  for (size_t k = 0; k < stationary_flag_.size(); ++k) {
    if (stationary_flag_[k] == false) {
      hangover_[k] = kHangoverBlocks;
    } else if (reduce_hangover) {
      hangover_[k] = std::max(hangover_[k] - 1, 0);
    }
  }
}

bool StationarityEstimator::EstimateBandStationarity(
    const std::vector<size_t>& idx_lookahead,
    const std::vector<size_t>& idx_lookback,
    size_t k) const {
  constexpr float kThrStationarity = 10.f;
  float acumPower = 0.f;
  for (auto it = idx_lookahead.cbegin(); it != idx_lookahead.end(); ++it) {
    acumPower += buffer_.GetPowerBand(*it, k);
  }
  for (auto it = idx_lookback.cbegin(); it != idx_lookback.cend(); ++it) {
    acumPower += buffer_.GetPowerBand(*it, k);
  }

  // Generally windowSize is equal to kLongWindowSize
  float windowSize = idx_lookahead.size() + idx_lookback.size();
  float noise = windowSize * GetStationarityPowerBand(k);
  bool stationary = ((acumPower / noise) < kThrStationarity);
  data_dumper_->DumpRaw("aec3_stationarity_long_ratio",
                        static_cast<float>(acumPower / noise));
  return stationary;
}

void StationarityEstimator::GetSlotsAheadBack(
    std::vector<size_t>* idx_lookahead,
    std::vector<size_t>* idx_lookback,
    size_t current_block_number) {
  for (size_t block = 0; block < idx_lookahead->size(); ++block) {
    (*idx_lookahead)[block] =
        buffer_.GetSlotNumber(current_block_number + block);
  }
  size_t num_back;
  if (idx_lookahead->size() >= kLongWindowSize) {
    RTC_CHECK_EQ(idx_lookahead->size(), kLongWindowSize);
    num_back = 0;
  } else {
    num_back = kLongWindowSize - idx_lookahead->size();
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
    all_ahead_stationary_smooth[k] = stationary_flag_[k - 1] &&
                                     stationary_flag_[k] &&
                                     stationary_flag_[k + 1];
  }

  all_ahead_stationary_smooth[0] = all_ahead_stationary_smooth[1];
  all_ahead_stationary_smooth[kFftLengthBy2Plus1 - 1] =
      all_ahead_stationary_smooth[kFftLengthBy2Plus1 - 2];

  stationary_flag_ = all_ahead_stationary_smooth;
}

StationarityEstimator::NoiseSpectrum::NoiseSpectrum() {
  Reset();
}

StationarityEstimator::NoiseSpectrum::~NoiseSpectrum() = default;

void StationarityEstimator::NoiseSpectrum::Reset() {
  block_counter_ = 0;
  noise_spectrum_.fill(kMinNoisePower);
}

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
}

StationarityEstimator::CircularBuffer::Element::Element() : block_number_(-1) {}

StationarityEstimator::CircularBuffer::CircularBuffer() = default;

bool StationarityEstimator::CircularBuffer::IsBlockNumberAlreadyUpdated(
    size_t block_number) const {
  size_t slot_number = GetSlotNumber(block_number);
  return (slots_[slot_number].GetBlockNumber() == block_number);
}

size_t StationarityEstimator::CircularBuffer::SetBlockNumberInSlot(
    size_t block_number) {
  size_t slot = GetSlotNumber(block_number);
  slots_[slot].SetBlockNumber(block_number);
  return slot;
}

}  // namespace webrtc
