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
#include <functional>

#if defined(WEBRTC_HAS_NEON)
#include <arm_neon.h>
#endif
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif

#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace aec3 {

void ComputeSpectralSum(const SpectrumBuffer& spectrum_buffer,
                        size_t num_spectra,
                        std::array<float, kFftLengthBy2Plus1>* X2) {
  X2->fill(0.f);
  int position = spectrum_buffer.read;
  for (size_t j = 0; j < num_spectra; ++j) {
    for (const std::array<float, kFftLengthBy2Plus1>& channel_spectrum :
         spectrum_buffer.buffer[position]) {
      std::transform(X2->begin(), X2->end(), channel_spectrum.begin(),
                     X2->begin(), std::plus<float>());
    }
    position = spectrum_buffer.IncIndex(position);
  }
}

#if defined(WEBRTC_HAS_NEON)

void ComputeSpectralSum_Neon(const SpectrumBuffer& spectrum_buffer,
                             size_t num_spectra,
                             std::array<float, kFftLengthBy2Plus1>* X2) {
  X2->fill(0.f);
  int position = spectrum_buffer.read;
  for (size_t j = 0; j < num_spectra; ++j) {
    for (const std::array<float, kFftLengthBy2Plus1>& channel_spectrum :
         spectrum_buffer.buffer[position]) {
      for (size_t k = 0; k < kFftLengthBy2; k += 4) {
        const float32x4_t P = vld1q_f32(&channel_spectrum[k]);
        float32x4_t X = vld1q_f32(&(*X2)[k]);
        X = vaddq_f32(X, P);
        vst1q_f32(&(*X2)[k], X);
      }

      (*X2)[kFftLengthBy2] += channel_spectrum[kFftLengthBy2];
    }
    position = position < spectrum_buffer.size - 1 ? position + 1 : 0;
  }
}

#endif

#if defined(WEBRTC_ARCH_X86_FAMILY)

void ComputeSpectralSum_Sse2(const SpectrumBuffer& spectrum_buffer,
                             size_t num_spectra,
                             std::array<float, kFftLengthBy2Plus1>* X2) {
  X2->fill(0.f);
  int position = spectrum_buffer.read;
  for (size_t j = 0; j < num_spectra; ++j) {
    for (const std::array<float, kFftLengthBy2Plus1>& channel_spectrum :
         spectrum_buffer.buffer[position]) {
      for (size_t k = 0; k < kFftLengthBy2; k += 4) {
        const __m128 P = _mm_loadu_ps(&channel_spectrum[k]);
        __m128 X = _mm_loadu_ps(&(*X2)[k]);
        X = _mm_add_ps(X, P);
        _mm_storeu_ps(&(*X2)[k], X);
      }

      (*X2)[kFftLengthBy2] += channel_spectrum[kFftLengthBy2];
    }
    position = position < spectrum_buffer.size - 1 ? position + 1 : 0;
  }
}

#endif

void ComputeSpectralSums(const SpectrumBuffer& spectrum_buffer,
                         size_t num_spectra_shorter,
                         size_t num_spectra_longer,
                         std::array<float, kFftLengthBy2Plus1>* X2_shorter,
                         std::array<float, kFftLengthBy2Plus1>* X2_longer) {
  RTC_DCHECK_LE(num_spectra_shorter, num_spectra_longer);
  X2_shorter->fill(0.f);
  int position = spectrum_buffer.read;
  size_t j = 0;
  for (; j < num_spectra_shorter; ++j) {
    for (const std::array<float, kFftLengthBy2Plus1>& channel_spectrum :
         spectrum_buffer.buffer[position]) {
      std::transform(X2_shorter->begin(), X2_shorter->end(),
                     channel_spectrum.begin(), X2_shorter->begin(),
                     std::plus<float>());
    }
    position = spectrum_buffer.IncIndex(position);
  }
  std::copy(X2_shorter->begin(), X2_shorter->end(), X2_longer->begin());
  for (; j < num_spectra_longer; ++j) {
    for (const std::array<float, kFftLengthBy2Plus1>& channel_spectrum :
         spectrum_buffer.buffer[position]) {
      std::transform(X2_longer->begin(), X2_longer->end(),
                     channel_spectrum.begin(), X2_longer->begin(),
                     std::plus<float>());
    }
    position = spectrum_buffer.IncIndex(position);
  }
}

#if defined(WEBRTC_HAS_NEON)

void ComputeSpectralSums_Neon(
    const SpectrumBuffer& spectrum_buffer,
    size_t num_spectra_shorter,
    size_t num_spectra_longer,
    std::array<float, kFftLengthBy2Plus1>* X2_shorter,
    std::array<float, kFftLengthBy2Plus1>* X2_longer) {
  RTC_DCHECK_LE(num_spectra_shorter, num_spectra_longer);
  X2_shorter->fill(0.f);
  int position = spectrum_buffer.read;
  size_t j = 0;
  for (; j < num_spectra_shorter; ++j) {
    for (const std::array<float, kFftLengthBy2Plus1>& channel_spectrum :
         spectrum_buffer.buffer[position]) {
      for (size_t k = 0; k < kFftLengthBy2; k += 4) {
        const float32x4_t P = vld1q_f32(&channel_spectrum[k]);
        float32x4_t X = vld1q_f32(&(*X2_shorter)[k]);
        X = vaddq_f32(X, P);
        vst1q_f32(&(*X2_shorter)[k], X);
      }

      (*X2_shorter)[kFftLengthBy2] += channel_spectrum[kFftLengthBy2];
    }
    position = position < spectrum_buffer.size - 1 ? position + 1 : 0;
  }
  std::copy(X2_shorter->begin(), X2_shorter->end(), X2_longer->begin());
  for (; j < num_spectra_longer; ++j) {
    for (const std::array<float, kFftLengthBy2Plus1>& channel_spectrum :
         spectrum_buffer.buffer[position]) {
      for (size_t k = 0; k < kFftLengthBy2; k += 4) {
        const float32x4_t P = vld1q_f32(&channel_spectrum[k]);
        float32x4_t X = vld1q_f32(&(*X2_longer)[k]);
        X = vaddq_f32(X, P);
        vst1q_f32(&(*X2_longer)[k], X);
      }

      (*X2_longer)[kFftLengthBy2] += channel_spectrum[kFftLengthBy2];
    }
    position = position < spectrum_buffer.size - 1 ? position + 1 : 0;
  }
}

#endif

#if defined(WEBRTC_ARCH_X86_FAMILY)

void ComputeSpectralSums_Sse2(
    const SpectrumBuffer& spectrum_buffer,
    size_t num_spectra_shorter,
    size_t num_spectra_longer,
    std::array<float, kFftLengthBy2Plus1>* X2_shorter,
    std::array<float, kFftLengthBy2Plus1>* X2_longer) {
  RTC_DCHECK_LE(num_spectra_shorter, num_spectra_longer);
  X2_shorter->fill(0.f);
  int position = spectrum_buffer.read;
  size_t j = 0;
  for (; j < num_spectra_shorter; ++j) {
    for (const std::array<float, kFftLengthBy2Plus1>& channel_spectrum :
         spectrum_buffer.buffer[position]) {
      for (size_t k = 0; k < kFftLengthBy2; k += 4) {
        const __m128 P = _mm_loadu_ps(&channel_spectrum[k]);
        __m128 X = _mm_loadu_ps(&(*X2_shorter)[k]);
        X = _mm_add_ps(X, P);
        _mm_storeu_ps(&(*X2_shorter)[k], X);
      }

      (*X2_shorter)[kFftLengthBy2] += channel_spectrum[kFftLengthBy2];
    }
    position = position < spectrum_buffer.size - 1 ? position + 1 : 0;
  }
  std::copy(X2_shorter->begin(), X2_shorter->end(), X2_longer->begin());
  for (; j < num_spectra_longer; ++j) {
    for (const std::array<float, kFftLengthBy2Plus1>& channel_spectrum :
         spectrum_buffer.buffer[position]) {
      for (size_t k = 0; k < kFftLengthBy2; k += 4) {
        const __m128 P = _mm_loadu_ps(&channel_spectrum[k]);
        __m128 X = _mm_loadu_ps(&(*X2_longer)[k]);
        X = _mm_add_ps(X, P);
        _mm_storeu_ps(&(*X2_longer)[k], X);
      }

      (*X2_longer)[kFftLengthBy2] += channel_spectrum[kFftLengthBy2];
    }
    position = position < spectrum_buffer.size - 1 ? position + 1 : 0;
  }
}

#endif

}  // namespace aec3

RenderBuffer::RenderBuffer(BlockBuffer* block_buffer,
                           SpectrumBuffer* spectrum_buffer,
                           FftBuffer* fft_buffer)
    : optimization_(DetectOptimization()),
      block_buffer_(block_buffer),
      spectrum_buffer_(spectrum_buffer),
      fft_buffer_(fft_buffer) {
  RTC_DCHECK(block_buffer_);
  RTC_DCHECK(spectrum_buffer_);
  RTC_DCHECK(fft_buffer_);
  RTC_DCHECK_EQ(block_buffer_->buffer.size(), fft_buffer_->buffer.size());
  RTC_DCHECK_EQ(spectrum_buffer_->buffer.size(), fft_buffer_->buffer.size());
  RTC_DCHECK_EQ(spectrum_buffer_->read, fft_buffer_->read);
  RTC_DCHECK_EQ(spectrum_buffer_->write, fft_buffer_->write);
}

RenderBuffer::~RenderBuffer() = default;

void RenderBuffer::SpectralSum(
    size_t num_spectra,
    std::array<float, kFftLengthBy2Plus1>* X2) const {
  switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
    case Aec3Optimization::kSse2:
      aec3::ComputeSpectralSum_Sse2(*spectrum_buffer_, num_spectra, X2);
      break;
#endif
#if defined(WEBRTC_HAS_NEON)
    case Aec3Optimization::kNeon:
      aec3::ComputeSpectralSum_Neon(*spectrum_buffer_, num_spectra, X2);
      break;
#endif
    default:
      aec3::ComputeSpectralSum(*spectrum_buffer_, num_spectra, X2);
  }
}

void RenderBuffer::SpectralSums(
    size_t num_spectra_shorter,
    size_t num_spectra_longer,
    std::array<float, kFftLengthBy2Plus1>* X2_shorter,
    std::array<float, kFftLengthBy2Plus1>* X2_longer) const {
  switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
    case Aec3Optimization::kSse2:
      aec3::ComputeSpectralSums_Sse2(*spectrum_buffer_, num_spectra_shorter,
                                     num_spectra_longer, X2_shorter, X2_longer);
      break;
#endif
#if defined(WEBRTC_HAS_NEON)
    case Aec3Optimization::kNeon:
      aec3::ComputeSpectralSums_Neon(*spectrum_buffer_, num_spectra_shorter,
                                     num_spectra_longer, X2_shorter, X2_longer);
      break;
#endif
    default:
      aec3::ComputeSpectralSums(*spectrum_buffer_, num_spectra_shorter,
                                num_spectra_longer, X2_shorter, X2_longer);
  }
}

}  // namespace webrtc
