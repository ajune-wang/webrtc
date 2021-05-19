/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/clipping_predictor.h"

#include <algorithm>
#include <memory>

#include "common_audio/include/audio_util.h"
#include "rtc_base/checks.h"

namespace webrtc {

ClippingPredictor::ClippingPredictor(size_t num_channels,
                                     size_t buffered_levels,
                                     size_t previous_buffered_levels,
                                     int clipping_threshold,
                                     int crest_factor_margin)
    : ch_buffers_(num_channels),
      buffered_levels_(buffered_levels),
      previous_buffered_levels_(previous_buffered_levels),
      clipping_threshold_(clipping_threshold),
      crest_factor_margin_(crest_factor_margin) {
  for (auto& buffer : ch_buffers_) {
    buffer = std::make_unique<LevelBuffer>(buffered_levels_ +
                                           previous_buffered_levels_);
  }
  RTC_DCHECK_LT(0, ch_buffers_.size());
}

void ClippingPredictor::Reset() {
  for (auto& buffer : ch_buffers_) {
    buffer->Reset();
  }
}

void ClippingPredictor::Initialize() {
  for (auto& buffer : ch_buffers_) {
    buffer->Initialize();
  }
}

bool ClippingPredictor::ProcessAudioFrame(const float* const* audio,
                                          size_t num_channels,
                                          size_t samples_per_channel) {
  RTC_DCHECK(audio);
  RTC_DCHECK_GT(samples_per_channel, 0);
  RTC_DCHECK_EQ(num_channels, ch_buffers_.size());
  for (size_t ch = 0; ch < num_channels; ++ch) {
    float sum_squares = 0.0f;
    float peak = std::fabs(audio[ch][0]);
    for (size_t i = 0; i < samples_per_channel; ++i) {
      const float sample = audio[ch][i];
      sum_squares += sample * sample;
      peak = std::max(std::fabs(sample), peak);
    }
    ch_buffers_[ch]->Push(
        {sum_squares / static_cast<float>(samples_per_channel), peak});
  }
  return true;
}

bool ClippingPredictor::PredictClippingEvent(size_t channel) const {
  const auto& buffer = ch_buffers_[channel];
  if (buffer->Size() < buffered_levels_ + previous_buffered_levels_) {
    return false;
  }
  const absl::optional<float> crest_factor =
      ComputeCrestFactor(channel, 0, buffered_levels_);
  const absl::optional<float> previous_crest_factor =
      ComputeCrestFactor(channel, previous_buffered_levels_, buffered_levels_);
  const absl::optional<float> peak_value =
      buffer->ComputePartialMax(0, buffered_levels_);
  if (crest_factor.has_value() && previous_crest_factor.has_value() &&
      peak_value.has_value()) {
    const bool crest_factor_change_high =
        *crest_factor < *previous_crest_factor - crest_factor_margin_;
    const bool peak_high = FloatS16ToDbfs(*peak_value) > clipping_threshold_;
    if (crest_factor_change_high && peak_high) {
      return true;
    }
  }
  return false;
}

absl::optional<float> ClippingPredictor::ProjectClippedPeakValue(
    size_t channel) const {
  const auto& buffer = ch_buffers_[channel];
  const absl::optional<float> crest_factor =
      ComputeCrestFactor(channel, buffered_levels_, previous_buffered_levels_);
  const absl::optional<float> sum_squares =
      buffer->ComputePartialAverage(0, buffered_levels_);
  const absl::optional<float> peak_value =
      buffer->ComputePartialMax(0, buffered_levels_);
  if (crest_factor.has_value() && sum_squares.has_value() &&
      peak_value.has_value()) {
    if (FloatS16ToDbfs(*peak_value) > clipping_threshold_) {
      return *crest_factor + FloatS16ToDbfs(std::sqrt(*sum_squares));
    }
  }
  return absl::nullopt;
}

void LevelBuffer::Initialize() {
  Reset();
  data_.reserve(buffer_max_length_);
}

void LevelBuffer::Push(Level level) {
  ++tail_;
  tail_ %= buffer_max_length_;
  if (data_.size() < buffer_max_length_) {
    data_.push_back(level);
  } else {
    data_[tail_] = level;
  }
}

absl::optional<float> LevelBuffer::ComputePartialAverage(
    size_t delay,
    size_t num_items) const {
  if (num_items < 1 || delay + num_items > data_.size()) {
    return absl::nullopt;
  }
  float sum = 0.f;
  for (size_t i = 0; i < num_items && i < data_.size(); ++i) {
    const size_t idx = (data_.size() + tail_ - delay - i) % data_.size();
    sum += data_[idx].average;
  }
  return sum / static_cast<float>(num_items);
}

absl::optional<float> LevelBuffer::ComputePartialMax(size_t delay,
                                                     size_t num_items) const {
  if (num_items < 1 || delay + num_items > data_.size()) {
    return absl::nullopt;
  }
  float max = 0.f;
  for (size_t i = 0; i < num_items && i < data_.size(); ++i) {
    const size_t idx = (data_.size() + tail_ - delay - i) % data_.size();
    max = std::fmax(data_[idx].max, max);
  }
  return max;
}

absl::optional<float> LevelBuffer::ComputeAverage() const {
  if (data_.empty()) {
    return absl::nullopt;
  }
  float sum = 0.0f;
  for (const auto& value : data_) {
    sum += value.average;
  }
  return sum / static_cast<float>(data_.size());
}

absl::optional<float> LevelBuffer::ComputeMax() const {
  if (data_.empty()) {
    return absl::nullopt;
  }
  float max = data_[0].max;
  for (const auto& value : data_) {
    max = std::fmax(value.max, max);
  }
  return max;
}

absl::optional<float> ClippingPredictor::ComputeCrestFactor(
    size_t channel,
    size_t delay,
    size_t num_frames) const {
  const absl::optional<float> sq_average =
      ch_buffers_[channel]->ComputePartialAverage(delay, num_frames);
  const absl::optional<float> abs_max =
      ch_buffers_[channel]->ComputePartialMax(delay, num_frames);
  if (sq_average.has_value() && abs_max.has_value()) {
    const float crest_factor =
        FloatS16ToDbfs(*abs_max) - FloatS16ToDbfs(std::sqrt(*sq_average));
    return absl::optional<float>(crest_factor);
  }
  return absl::nullopt;
}

}  // namespace webrtc
