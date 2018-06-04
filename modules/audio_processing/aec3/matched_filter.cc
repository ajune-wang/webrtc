/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/matched_filter.h"

#if defined(WEBRTC_HAS_NEON)
#include <arm_neon.h>
#endif
#include "typedefs.h"  // NOLINT(build/include)
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include <algorithm>
#include <numeric>

#include "api/array_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace aec3 {

#if defined(WEBRTC_HAS_NEON)

void MatchedFilterCore_NEON(size_t x_start_index,
                            float x2_sum_threshold,
                            float step_size,
                            rtc::ArrayView<const float> x,
                            rtc::ArrayView<const float> y,
                            rtc::ArrayView<float> h,
                            bool* filters_updated,
                            float* error_sum) {
  const int h_size = static_cast<int>(h.size());
  const int x_size = static_cast<int>(x.size());
  RTC_DCHECK_EQ(0, h_size % 4);

  // Process for all samples in the sub-block.
  for (size_t i = 0; i < y.size(); ++i) {
    // Apply the matched filter as filter * x, and compute x * x.

    RTC_DCHECK_GT(x_size, x_start_index);
    const float* x_p = &x[x_start_index];
    const float* h_p = &h[0];

    // Initialize values for the accumulation.
    float32x4_t s_128 = vdupq_n_f32(0);
    float32x4_t x2_sum_128 = vdupq_n_f32(0);
    float x2_sum = 0.f;
    float s = 0;

    // Compute loop chunk sizes until, and after, the wraparound of the circular
    // buffer for x.
    const int chunk1 =
        std::min(h_size, static_cast<int>(x_size - x_start_index));

    // Perform the loop in two chunks.
    const int chunk2 = h_size - chunk1;
    for (int limit : {chunk1, chunk2}) {
      // Perform 128 bit vector operations.
      const int limit_by_4 = limit >> 2;
      for (int k = limit_by_4; k > 0; --k, h_p += 4, x_p += 4) {
        // Load the data into 128 bit vectors.
        const float32x4_t x_k = vld1q_f32(x_p);
        const float32x4_t h_k = vld1q_f32(h_p);
        // Compute and accumulate x * x and h * x.
        x2_sum_128 = vmlaq_f32(x2_sum_128, x_k, x_k);
        s_128 = vmlaq_f32(s_128, h_k, x_k);
      }

      // Perform non-vector operations for any remaining items.
      for (int k = limit - limit_by_4 * 4; k > 0; --k, ++h_p, ++x_p) {
        const float x_k = *x_p;
        x2_sum += x_k * x_k;
        s += *h_p * x_k;
      }

      x_p = &x[0];
    }

    // Combine the accumulated vector and scalar values.
    float* v = reinterpret_cast<float*>(&x2_sum_128);
    x2_sum += v[0] + v[1] + v[2] + v[3];
    v = reinterpret_cast<float*>(&s_128);
    s += v[0] + v[1] + v[2] + v[3];

    // Compute the matched filter error.
    float e = y[i] - s;
    const bool saturation = y[i] >= 32000.f || y[i] <= -32000.f ||
                            s >= 32000.f || s <= -32000.f || e >= 32000.f ||
                            e <= -32000.f;

    e = std::min(32767.f, std::max(-32768.f, e));
    (*error_sum) += e * e;

    // Update the matched filter estimate in an NLMS manner.
    if (x2_sum > x2_sum_threshold && !saturation) {
      RTC_DCHECK_LT(0.f, x2_sum);
      const float alpha = step_size * e / x2_sum;
      const float32x4_t alpha_128 = vmovq_n_f32(alpha);

      // filter = filter + alpha * (y - filter * x) / x * x.
      float* h_p = &h[0];
      x_p = &x[x_start_index];

      // Perform the loop in two chunks.
      for (int limit : {chunk1, chunk2}) {
        // Perform 128 bit vector operations.
        const int limit_by_4 = limit >> 2;
        for (int k = limit_by_4; k > 0; --k, h_p += 4, x_p += 4) {
          // Load the data into 128 bit vectors.
          float32x4_t h_k = vld1q_f32(h_p);
          const float32x4_t x_k = vld1q_f32(x_p);
          // Compute h = h + alpha * x.
          h_k = vmlaq_f32(h_k, alpha_128, x_k);

          // Store the result.
          vst1q_f32(h_p, h_k);
        }

        // Perform non-vector operations for any remaining items.
        for (int k = limit - limit_by_4 * 4; k > 0; --k, ++h_p, ++x_p) {
          *h_p += alpha * *x_p;
        }

        x_p = &x[0];
      }

      *filters_updated = true;
    }

    x_start_index = x_start_index > 0 ? x_start_index - 1 : x_size - 1;
  }
}

#endif

#if defined(WEBRTC_ARCH_X86_FAMILY)

void MatchedFilterCore_SSE2(size_t x_start_index,
                            float x2_sum_threshold,
                            float step_size,
                            rtc::ArrayView<const float> x,
                            rtc::ArrayView<const float> y,
                            rtc::ArrayView<float> h,
                            bool* filters_updated,
                            float* error_sum) {
  const int h_size = static_cast<int>(h.size());
  const int x_size = static_cast<int>(x.size());
  RTC_DCHECK_EQ(0, h_size % 4);

  // Process for all samples in the sub-block.
  for (size_t i = 0; i < y.size(); ++i) {
    // Apply the matched filter as filter * x, and compute x * x.

    RTC_DCHECK_GT(x_size, x_start_index);
    const float* x_p = &x[x_start_index];
    const float* h_p = &h[0];

    // Initialize values for the accumulation.
    __m128 s_128 = _mm_set1_ps(0);
    __m128 x2_sum_128 = _mm_set1_ps(0);
    float x2_sum = 0.f;
    float s = 0;

    // Compute loop chunk sizes until, and after, the wraparound of the circular
    // buffer for x.
    const int chunk1 =
        std::min(h_size, static_cast<int>(x_size - x_start_index));

    // Perform the loop in two chunks.
    const int chunk2 = h_size - chunk1;
    for (int limit : {chunk1, chunk2}) {
      // Perform 128 bit vector operations.
      const int limit_by_4 = limit >> 2;
      for (int k = limit_by_4; k > 0; --k, h_p += 4, x_p += 4) {
        // Load the data into 128 bit vectors.
        const __m128 x_k = _mm_loadu_ps(x_p);
        const __m128 h_k = _mm_loadu_ps(h_p);
        const __m128 xx = _mm_mul_ps(x_k, x_k);
        // Compute and accumulate x * x and h * x.
        x2_sum_128 = _mm_add_ps(x2_sum_128, xx);
        const __m128 hx = _mm_mul_ps(h_k, x_k);
        s_128 = _mm_add_ps(s_128, hx);
      }

      // Perform non-vector operations for any remaining items.
      for (int k = limit - limit_by_4 * 4; k > 0; --k, ++h_p, ++x_p) {
        const float x_k = *x_p;
        x2_sum += x_k * x_k;
        s += *h_p * x_k;
      }

      x_p = &x[0];
    }

    // Combine the accumulated vector and scalar values.
    float* v = reinterpret_cast<float*>(&x2_sum_128);
    x2_sum += v[0] + v[1] + v[2] + v[3];
    v = reinterpret_cast<float*>(&s_128);
    s += v[0] + v[1] + v[2] + v[3];

    // Compute the matched filter error.
    float e = y[i] - s;
    const bool saturation = y[i] >= 32000.f || y[i] <= -32000.f ||
                            s >= 32000.f || s <= -32000.f || e >= 32000.f ||
                            e <= -32000.f;

    e = std::min(32767.f, std::max(-32768.f, e));
    (*error_sum) += e * e;

    // Update the matched filter estimate in an NLMS manner.
    if (x2_sum > x2_sum_threshold && !saturation) {
      RTC_DCHECK_LT(0.f, x2_sum);
      const float alpha = step_size * e / x2_sum;
      const __m128 alpha_128 = _mm_set1_ps(alpha);

      // filter = filter + alpha * (y - filter * x) / x * x.
      float* h_p = &h[0];
      x_p = &x[x_start_index];

      // Perform the loop in two chunks.
      for (int limit : {chunk1, chunk2}) {
        // Perform 128 bit vector operations.
        const int limit_by_4 = limit >> 2;
        for (int k = limit_by_4; k > 0; --k, h_p += 4, x_p += 4) {
          // Load the data into 128 bit vectors.
          __m128 h_k = _mm_loadu_ps(h_p);
          const __m128 x_k = _mm_loadu_ps(x_p);

          // Compute h = h + alpha * x.
          const __m128 alpha_x = _mm_mul_ps(alpha_128, x_k);
          h_k = _mm_add_ps(h_k, alpha_x);

          // Store the result.
          _mm_storeu_ps(h_p, h_k);
        }

        // Perform non-vector operations for any remaining items.
        for (int k = limit - limit_by_4 * 4; k > 0; --k, ++h_p, ++x_p) {
          *h_p += alpha * *x_p;
        }

        x_p = &x[0];
      }

      *filters_updated = true;
    }

    x_start_index = x_start_index > 0 ? x_start_index - 1 : x_size - 1;
  }
}
#endif

void MatchedFilterCore(size_t x_start_index,
                       float x2_sum_threshold,
                       float step_size,
                       rtc::ArrayView<const float> x,
                       rtc::ArrayView<const float> y,
                       rtc::ArrayView<float> h,
                       bool* filters_updated,
                       float* error_sum) {
  // Process for all samples in the sub-block.
  for (size_t i = 0; i < y.size(); ++i) {
    // Apply the matched filter as filter * x, and compute x * x.
    float x2_sum = 0.f;
    float s = 0;
    size_t x_index = x_start_index;
    for (size_t k = 0; k < h.size(); ++k) {
      x2_sum += x[x_index] * x[x_index];
      s += h[k] * x[x_index];
      x_index = x_index < (x.size() - 1) ? x_index + 1 : 0;
    }

    // Compute the matched filter error.
    float e = y[i] - s;
    const bool saturation = y[i] >= 32000.f || y[i] <= -32000.f ||
                            s >= 32000.f || s <= -32000.f || e >= 32000.f ||
                            e <= -32000.f;

    e = std::min(32767.f, std::max(-32768.f, e));
    (*error_sum) += e * e;

    // Update the matched filter estimate in an NLMS manner.
    if (x2_sum > x2_sum_threshold && !saturation) {
      RTC_DCHECK_LT(0.f, x2_sum);
      const float alpha = step_size * e / x2_sum;

      // filter = filter + alpha * (y - filter * x) / x * x.
      size_t x_index = x_start_index;
      for (size_t k = 0; k < h.size(); ++k) {
        h[k] += alpha * x[x_index];
        x_index = x_index < (x.size() - 1) ? x_index + 1 : 0;
      }
      *filters_updated = true;
    }

    x_start_index = x_start_index > 0 ? x_start_index - 1 : x.size() - 1;
  }
}

void Filter16x8(const DownsampledRenderBuffer& render_buffer,
                rtc::ArrayView<const float> y,
                float x2_sum_threshold,
                float step_size,
                rtc::ArrayView<float> x2_sum,
                rtc::ArrayView<float> e,
                rtc::ArrayView<float> e2_sum,
                std::vector<bool>* filters_updated,
                rtc::ArrayView<float> h) {
  RTC_DCHECK_EQ(0, h.size() % 16);
  const size_t num_filters = h.size() / 16 / 8;
  const std::vector<float>& x = render_buffer.buffer;
  RTC_DCHECK_EQ(0, x.size() % 4);
  RTC_DCHECK_EQ(0, render_buffer.read % 4);
  RTC_DCHECK_EQ(num_filters, x2_sum.size());
  RTC_DCHECK_EQ(num_filters, e2_sum.size());
  RTC_DCHECK_EQ(num_filters, e.size());
  RTC_DCHECK_EQ(num_filters, filters_updated->size());

  size_t x0 = render_buffer.read;
  const size_t x_size_minus_1 = x.size() - 1;

  std::fill(e2_sum.begin(), e2_sum.end(), 0.f);
  std::fill(filters_updated->begin(), filters_updated->end(), false);
  for (size_t i = 0; i < y.size(); ++i) {
    std::fill(x2_sum.begin(), x2_sum.end(), 0.f);
    std::fill(e.begin(), e.end(), 0.f);

    size_t x_i = x0;
    size_t h_i = 0;
    for (size_t k = 0; k < 16; ++k) {
      for (size_t n = 0; n < 4; ++n) {
        x2_sum[0] += x[x_i + n] * x[x_i + n];
        e[0] += h[h_i + n] * x[x_i + n];
      }
      h_i += 4;
      x_i = (x_i + 4) & x_size_minus_1;
    }

    for (size_t j = 0; j < num_filters - 1; ++j) {
      for (size_t k = 0; k < 16; ++k) {
        for (size_t n = 0; n < 4; ++n) {
          float x2 = x[x_i + n] * x[x_i + n];
          x2_sum[j]     += x2;
          x2_sum[j + 1] += x2;
          e[j]     += h[h_i     + n] * x[x_i + n];
          e[j + 1] += h[h_i + 4 + n] * x[x_i + n];
        }
        h_i += 8;
        x_i = (x_i + 4) & x_size_minus_1;
      }
    }

    for (size_t k = 0; k < 16; ++k) {
      for (size_t n = 0; n < 4; ++n) {
        x2_sum[num_filters - 1] += x[x_i + n] * x[x_i + n];
        e[num_filters - 1] += h[h_i + n] * x[x_i + n];
      }
      h_i += 4;
      x_i = (x_i + 4) & x_size_minus_1;
    }
    RTC_DCHECK_EQ(h_i, h.size());

    const bool saturation = y[y.size() - 1 - i] >= 32000.f || y[y.size() - 1 - i] <= -32000.f;
    for (size_t j = 0; j < num_filters; ++j) {
      e[j] = y[y.size() - 1 - i] - e[j];
      e2_sum[j] += e[j] * e[j];

      if (x2_sum[j] > x2_sum_threshold && !saturation) {
        e[j] *= step_size / x2_sum[j];
        (*filters_updated)[j] = true;
      } else {
        e[j] = 0.f;
      }
    }

    x_i = x0;
    h_i = 0;
    for (size_t k = 0; k < 16; ++k) {
      for (size_t n = 0; n < 4; ++n) {
        h[h_i + n] += e[0] * x[x_i + n];
      }
      h_i += 4;
      x_i = (x_i + 4) & x_size_minus_1;
    }

    for (size_t j = 0; j < num_filters - 1; ++j) {
      for (size_t k = 0; k < 16; ++k) {
        for (size_t n = 0; n < 4; ++n) {
          h[h_i     + n] += e[j]     * x[x_i + n];
          h[h_i + 4 + n] += e[j + 1] * x[x_i + n];
        }
        h_i += 8;
        x_i = (x_i + 4) & x_size_minus_1;
      }
    }

    for (size_t k = 0; k < 16; ++k) {
      for (size_t n = 0; n < 4; ++n) {
        h[h_i + n] += e[num_filters - 1] * x[x_i + n];
      }
      h_i += 4;
      x_i = (x_i + 4) & x_size_minus_1;
    }
    RTC_DCHECK_EQ(h_i, h.size());

    x0 = (x0 + 1) & x_size_minus_1;
  }
}

void FindPeaks16x8(rtc::ArrayView<const float> h,
                   rtc::ArrayView<float> m,
                   rtc::ArrayView<size_t> peaks) {
  const size_t num_filters = h.size() / 16 / 8;
  RTC_DCHECK_EQ(num_filters, m.size());
  RTC_DCHECK_EQ(num_filters, peaks.size());

  std::fill(m.begin(), m.end(), 0.f);
  std::fill(peaks.begin(), peaks.end(), 0);

  int index1 = 0;
  size_t h_i = 0;
  for (size_t k = 0; k < 16; ++k) {
    for (size_t n = 0; n < 4; ++n) {
      float tmp = h[h_i + n] * h[h_i + n];
      if (tmp > m[0]) {
        m[0] = tmp;
        peaks[0] = index1;
      }
      ++index1;
    }
    h_i += 4;
  }
  RTC_DCHECK_EQ(64, index1);

  index1 = 64;
  int index2 = 0;
  for (size_t j = 0; j < num_filters - 1; ++j) {
    for (size_t k = 0; k < 16; ++k) {
      for (size_t n = 0; n < 4; ++n) {
        float tmp = h[h_i + n] * h[h_i + n];
        if (tmp > m[j]) {
          m[j] = tmp;
          peaks[j] = index1;
        }

        tmp = h[h_i + 4 + n] * h[h_i + 4 + n];
        if (tmp > m[j + 1]) {
          m[j + 1] = tmp;
          peaks[j + 1] = index2;
        }

        ++index1;
        ++index2;
      }
      h_i += 8;
    }
    RTC_DCHECK_EQ(128, index1);
    RTC_DCHECK_EQ(64, index2);
    index1 = 64;
    index2 = 0;
  }

  for (size_t k = 0; k < 16; ++k) {
    for (size_t n = 0; n < 4; ++n) {
      float tmp = h[h_i + n] * h[h_i + n];
      if (tmp > m[num_filters - 1]) {
        m[num_filters - 1] = tmp;
        peaks[num_filters - 1] = index1;
      }
      ++index1;
    }
    h_i += 4;
  }
  RTC_DCHECK_EQ(128, index1);
}

}  // namespace aec3

MatchedFilter::MatchedFilter(
    ApmDataDumper* data_dumper,
    Aec3Optimization optimization,
    const EchoCanceller3Config::Delay::MatchedFilters& config,
    size_t sub_block_size)
    : data_dumper_(data_dumper),
      optimization_(optimization),
      config_(config),
      sub_block_size_(sub_block_size),
      detection_threshold_(config_.detection_threshold),
      filter_intra_lag_shift_((config_.filter_size_sub_blocks -
                               config_.filter_alignment_overlap_sub_blocks) *
                              sub_block_size_),
      filters_(
          config_.num_filters,
          std::vector<float>(config_.filter_size_sub_blocks * sub_block_size_,
                             0.f)),
      lag_estimates_(config_.num_filters),
      filters_offsets_(config_.num_filters, 0),
      excitation_limit_(config.down_sampling_factor== 8? config_.poor_excitation_render_limit_ds8:config_.poor_excitation_render_limit) {
  RTC_DCHECK(data_dumper);
  RTC_DCHECK_LT(0, config_.filter_size_sub_blocks);
  RTC_DCHECK((kBlockSize % sub_block_size) == 0);
  RTC_DCHECK((sub_block_size % 4) == 0);

  if (config_.filter_size_sub_blocks == 16 &&
      config_.filter_alignment_overlap_sub_blocks == 8) {
    x2_sum_.resize(config_.num_filters);
    std::fill(x2_sum_.begin(), x2_sum_.end(), 0.f);

    e_.resize(config_.num_filters);
    std::fill(e_.begin(), e_.end(), 0.f);

    e2_sum_.resize(config_.num_filters);
    std::fill(e2_sum_.begin(), e2_sum_.end(), 0.f);

    peaks_.resize(config_.num_filters);
    std::fill(peaks_.begin(), peaks_.end(), 0);

    filters_updated_.resize(config_.num_filters);
    std::fill(filters_updated_.begin(), filters_updated_.end(), true);

    filters16x8_.resize(config_.num_filters * config_.filter_size_sub_blocks *
                        sub_block_size_);
    std::fill(filters16x8_.begin(), filters16x8_.end(), 0.f);

    std::vector<std::vector<float>> tmp(0, std::vector<float>(0));
    filters_.swap(tmp);
  }
}

MatchedFilter::~MatchedFilter() = default;

void MatchedFilter::Reset() {
  for (auto& f : filters_) {
    std::fill(f.begin(), f.end(), 0.f);
  }

  for (auto& l : lag_estimates_) {
    l = MatchedFilter::LagEstimate();
  }

  if (config_.filter_size_sub_blocks == 16 &&
      config_.filter_alignment_overlap_sub_blocks == 8) {
    std::fill(x2_sum_.begin(), x2_sum_.end(), 0.f);
    std::fill(e_.begin(), e_.end(), 0.f);
    std::fill(e2_sum_.begin(), e2_sum_.end(), 0.f);
    std::fill(peaks_.begin(), peaks_.end(), 0);
    std::fill(filters_updated_.begin(), filters_updated_.end(), true);
    std::fill(filters16x8_.begin(), filters16x8_.end(), 0.f);
  }
}

void MatchedFilter::Update(const DownsampledRenderBuffer& render_buffer,
                           rtc::ArrayView<const float> capture) {
  if (config_.filter_size_sub_blocks == 16 &&
      config_.filter_alignment_overlap_sub_blocks == 8) {
    Update16x8(render_buffer, capture);
  } else {
    UpdateGeneric(render_buffer, capture);
  }
}

void MatchedFilter::Update16x8(const DownsampledRenderBuffer& render_buffer,
                               rtc::ArrayView<const float> capture) {
  RTC_DCHECK_EQ(sub_block_size_, capture.size());
  auto& y = capture;

  const float x2_sum_threshold = excitation_limit_ * excitation_limit_ *
                                 sub_block_size_ *
                                 config_.filter_size_sub_blocks;

  aec3::Filter16x8(render_buffer, capture, x2_sum_threshold,
                   config_.estimator_smoothing, x2_sum_, e_, e2_sum_,
                   &filters_updated_, filters16x8_);

  aec3::FindPeaks16x8(filters16x8_, e_, peaks_);

  // Compute anchor for the matched filter error.
  const float error_sum_anchor =
      std::inner_product(y.begin(), y.end(), y.begin(), 0.f);

  size_t lag_estimate_bound =
      sub_block_size_ * config_.filter_size_sub_blocks - 10;
  size_t intra_filter_shift =
      sub_block_size_ * config_.filter_alignment_overlap_sub_blocks;
  for (size_t j = 0; j < config_.num_filters; ++j) {
    // Update the lag estimates for the matched filter.
    lag_estimates_[j] =
        LagEstimate(error_sum_anchor - e2_sum_[j],
                    (peaks_[j] > 2 && peaks_[j] < lag_estimate_bound &&
                     e2_sum_[j] < detection_threshold_ * error_sum_anchor),
                    peaks_[j] + j * intra_filter_shift, filters_updated_[j]);
  }
  data_dumper_->DumpRaw("aec3_correlator_h_16x8", filters16x8_);
}

void MatchedFilter::UpdateGeneric(const DownsampledRenderBuffer& render_buffer,
                                  rtc::ArrayView<const float> capture) {
  RTC_DCHECK_EQ(sub_block_size_, capture.size());
  auto& y = capture;

  const float x2_sum_threshold =
      filters_[0].size() * excitation_limit_ * excitation_limit_;

  // Apply all matched filters.
  size_t alignment_shift = 0;
  for (size_t n = 0; n < filters_.size(); ++n) {
    float error_sum = 0.f;
    bool filters_updated = false;

    size_t x_start_index =
        (render_buffer.read + alignment_shift + sub_block_size_ - 1) %
        render_buffer.buffer.size();

    switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
      case Aec3Optimization::kSse2:
        aec3::MatchedFilterCore_SSE2(
            x_start_index, x2_sum_threshold, config_.estimator_smoothing,
            render_buffer.buffer, y, filters_[n], &filters_updated, &error_sum);
        break;
#endif
#if defined(WEBRTC_HAS_NEON)
      case Aec3Optimization::kNeon:
        aec3::MatchedFilterCore_NEON(
            x_start_index, x2_sum_threshold, config_.estimator_smoothing,
            render_buffer.buffer, y, filters_[n], &filters_updated, &error_sum);
        break;
#endif
      default:
        aec3::MatchedFilterCore(
            x_start_index, x2_sum_threshold, config_.estimator_smoothing,
            render_buffer.buffer, y, filters_[n], &filters_updated, &error_sum);
    }

    // Compute anchor for the matched filter error.
    const float error_sum_anchor =
        std::inner_product(y.begin(), y.end(), y.begin(), 0.f);

    // Estimate the lag in the matched filter as the distance to the portion in
    // the filter that contributes the most to the matched filter output. This
    // is detected as the peak of the matched filter.
    const size_t lag_estimate = std::distance(
        filters_[n].begin(),
        std::max_element(
            filters_[n].begin(), filters_[n].end(),
            [](float a, float b) -> bool { return a * a < b * b; }));

    // Update the lag estimates for the matched filter.
    lag_estimates_[n] = LagEstimate(
        error_sum_anchor - error_sum,
        (lag_estimate > 2 && lag_estimate < (filters_[n].size() - 10) &&
         error_sum < detection_threshold_ * error_sum_anchor),
        lag_estimate + alignment_shift, filters_updated);

    RTC_DCHECK_GE(10, filters_.size());
    switch (n) {
      case 0:
        data_dumper_->DumpRaw("aec3_correlator_0_h", filters_[0]);
        break;
      case 1:
        data_dumper_->DumpRaw("aec3_correlator_1_h", filters_[1]);
        break;
      case 2:
        data_dumper_->DumpRaw("aec3_correlator_2_h", filters_[2]);
        break;
      case 3:
        data_dumper_->DumpRaw("aec3_correlator_3_h", filters_[3]);
        break;
      case 4:
        data_dumper_->DumpRaw("aec3_correlator_4_h", filters_[4]);
        break;
      case 5:
        data_dumper_->DumpRaw("aec3_correlator_5_h", filters_[5]);
        break;
      case 6:
        data_dumper_->DumpRaw("aec3_correlator_6_h", filters_[6]);
        break;
      case 7:
        data_dumper_->DumpRaw("aec3_correlator_7_h", filters_[7]);
        break;
      case 8:
        data_dumper_->DumpRaw("aec3_correlator_8_h", filters_[8]);
        break;
      case 9:
        data_dumper_->DumpRaw("aec3_correlator_9_h", filters_[9]);
        break;
      default:
        RTC_NOTREACHED();
    }

    alignment_shift += filter_intra_lag_shift_;
  }
}

void MatchedFilter::LogFilterProperties(int sample_rate_hz,
                                        size_t shift,
                                        size_t downsampling_factor) const {
  size_t alignment_shift = 0;
  const int fs_by_1000 = LowestBandRate(sample_rate_hz) / 1000;
  for (size_t k = 0; k < filters_.size(); ++k) {
    int start = static_cast<int>(alignment_shift * downsampling_factor);
    int end = static_cast<int>((alignment_shift + filters_[k].size()) *
                               downsampling_factor);
    RTC_LOG(LS_INFO) << "Filter " << k << ": start: "
                     << (start - static_cast<int>(shift)) / fs_by_1000
                     << " ms, end: "
                     << (end - static_cast<int>(shift)) / fs_by_1000 << " ms.";
    alignment_shift += filter_intra_lag_shift_;
  }
}

}  // namespace webrtc
