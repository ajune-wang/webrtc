/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_buffer.h"

#include <algorithm>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/checks.h"

namespace webrtc {

RenderBuffer::RenderBuffer(Aec3Optimization optimization,
                           size_t num_bands,
                           size_t num_partitions,
                           const std::vector<size_t> num_ffts_for_spectral_sums,
                           aec3::MatrixBuffer* block_buffer,
                           FftBuffer* fft_buffer)
    : optimization_(optimization),
      block_buffer_(block_buffer),
      fft_buffer_(fft_buffer),
      spectrum_buffer_(num_partitions, std::array<float, kFftLengthBy2Plus1>()),
      spectral_sums_(num_ffts_for_spectral_sums.size(),
                     std::array<float, kFftLengthBy2Plus1>()),
      fft_() {
  RTC_DCHECK(block_buffer_);
  RTC_DCHECK(fft_buffer_);
  // Current implementation only allows a maximum of one spectral sum lengths.
  RTC_DCHECK_EQ(1, num_ffts_for_spectral_sums.size());
  spectral_sums_length_ = num_ffts_for_spectral_sums[0];
  RTC_DCHECK_GE(fft_buffer_->buffer.size(), spectral_sums_length_);
  (void)block_buffer_;
  Clear();
}

RenderBuffer::~RenderBuffer() = default;

void RenderBuffer::Clear() {
  position_ = 0;
  for (auto& sum : spectral_sums_) {
    sum.fill(0.f);
  }
  for (auto& spectrum : spectrum_buffer_) {
    spectrum.fill(0.f);
  }
}

void RenderBuffer::Insert(const std::vector<std::vector<float>>& block2) {
  // Compute and insert the spectrum for the FFT into the spectrum buffer.
  position_ = position_ > 0 ? position_ - 1 : spectrum_buffer_.size() - 1;
  fft_buffer_->buffer[fft_buffer_->next_read_index].Spectrum(
      optimization_, &spectrum_buffer_[position_]);

  // Pre-compute and cache the spectral sums.
  std::copy(spectrum_buffer_[position_].begin(),
            spectrum_buffer_[position_].end(), spectral_sums_[0].begin());

  size_t position = (position_ + 1) % spectrum_buffer_.size();
  for (size_t j = 1; j < spectral_sums_length_; ++j) {
    const std::array<float, kFftLengthBy2Plus1>& spectrum =
        spectrum_buffer_[position];

    for (size_t k = 0; k < spectral_sums_[0].size(); ++k) {
      spectral_sums_[0][k] += spectrum[k];
    }

    position = position < (spectrum_buffer_.size() - 1) ? position + 1 : 0;
  }
}

}  // namespace webrtc
