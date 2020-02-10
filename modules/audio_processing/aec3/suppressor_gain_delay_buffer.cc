/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/suppressor_gain_delay_buffer.h"

#include <algorithm>

namespace webrtc {

namespace {

size_t GetDelayBuffersSize(int delay_blocks, float fractional_delay_blocks) {
  RTC_DCHECK_LE(0, delay_blocks);
  if (delay_blocks == 0 && fractional_delay_blocks == 0.f) {
    return 0;
  }

  return 1 + delay_blocks + (fractional_delay_blocks == 0.f ? 0 : 1);
}

}  // namespace

SuppressorGainDelayBuffer::SuppressorGainDelayBuffer(float delay_ms)
    : delay_blocks_(delay_ms / 4),
      fractional_delay_blocks_((delay_ms - 4.f * delay_blocks_) / 4.f),
      low_band_gain_buffer_(
          GetDelayBuffersSize(delay_blocks_, fractional_delay_blocks_)),
      high_bands_gain_buffer_(low_band_gain_buffer_.size()) {}

void SuppressorGainDelayBuffer::Delay(
    rtc::ArrayView<float, kFftLengthBy2Plus1> low_band_gains,
    float* high_bands_gain) {
  if (delay_blocks_ == 0 && fractional_delay_blocks_ == 0.f) {
    return;
  }

  size_t next_insert_index = last_insert_index_ > 0
                                 ? last_insert_index_ - 1
                                 : low_band_gain_buffer_.size() - 1;
  high_bands_gain_buffer_[next_insert_index] = *high_bands_gain;
  std::copy(low_band_gains.begin(), low_band_gains.end(),
            low_band_gain_buffer_[next_insert_index].begin());

  size_t next_extract_index =
      (next_insert_index + delay_blocks_) % low_band_gain_buffer_.size();
  if (fractional_delay_blocks_ == 0.f) {
    std::copy(low_band_gain_buffer_[next_extract_index].begin(),
              low_band_gain_buffer_[next_extract_index].end(),
              low_band_gains.begin());
    *high_bands_gain = high_bands_gain_buffer_[next_extract_index];
  } else {
    size_t prev_extract_index =
        (next_insert_index + delay_blocks_ + 1) % low_band_gain_buffer_.size();

    const float factor_older = fractional_delay_blocks_;
    const float factor_newer = 1.f - fractional_delay_blocks_;

    *high_bands_gain =
        factor_newer * high_bands_gain_buffer_[next_extract_index] +
        factor_older * high_bands_gain_buffer_[prev_extract_index];

    for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
      low_band_gains[k] =
          factor_newer * low_band_gain_buffer_[next_extract_index][k] +
          factor_older * low_band_gain_buffer_[prev_extract_index][k];
    }
  }

  last_insert_index_ = next_insert_index;
  all_buffer_populated_ = all_buffer_populated_ || (last_insert_index_ == 0);
}

}  // namespace webrtc
