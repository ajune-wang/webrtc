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

#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/logging.h"

namespace webrtc {

// size_t GetMatchedFilterSize() { return  32; }
// size_t GetMatchedFilterAlignment() { return 24; }
size_t GetMatchedFilterSize() {
  return 16;
}
size_t GetMatchedFilterAlignment() {
  return 8;
}

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

      // filter = filter + 0.7 * (y - filter * x) / x * x.
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

      // filter = filter + 0.7 * (y - filter * x) / x * x.
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

      // filter = filter + 0.7 * (y - filter * x) / x * x.
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

void FilterSymmetricOverlap_SSE2(rtc::ArrayView<const float> x,
                                 size_t x_start,
                                 rtc::ArrayView<const float> y,
                                 size_t filter_size,
                                 size_t num_filters,
                                 float x2_sum_threshold,
                                 float step_size,
                                 rtc::ArrayView<float> x2_sum,
                                 rtc::ArrayView<float> e,
                                 rtc::ArrayView<float> e2_sum,
                                 std::vector<bool>* filters_updated,
                                 rtc::ArrayView<float> h) {
  RTC_DCHECK_EQ(0, h.size() % 4);
  RTC_DCHECK_EQ(0, x.size() % 4);
  RTC_DCHECK_EQ(0, x_start % 4);
  RTC_DCHECK_EQ(num_filters, x2_sum.size());
  RTC_DCHECK_EQ(num_filters, e2_sum.size());
  RTC_DCHECK_EQ(num_filters, e.size());
  RTC_DCHECK_EQ(num_filters, filters_updated->size());

  size_t x0 = x_start;

  int filter_size_by_2 = static_cast<int>(filter_size >> 1);

  std::fill(e2_sum.begin(), e2_sum.end(), 0.f);
  std::fill(filters_updated->begin(), filters_updated->end(), false);
  for (size_t i = 0; i < y.size(); ++i) {
    size_t x_i = x0;
    float* h_p = &h[0];
    __m128 x2_sum_128_j = _mm_set1_ps(0);
    __m128 e_128_j = _mm_set1_ps(0);

    int chunk1 = std::min(filter_size_by_2, static_cast<int>(x.size() - x_i));
    int chunk2 = filter_size_by_2 - chunk1;

    for (int limit : {chunk1, chunk2}) {
      const int limit_by_4 = limit >> 2;
      for (int k = 0; k < limit_by_4; ++k) {
        const __m128 x_k = _mm_loadu_ps(&x[x_i]);
        const __m128 h_k = _mm_loadu_ps(h_p);
        const __m128 xx = _mm_mul_ps(x_k, x_k);
        x2_sum_128_j = _mm_add_ps(x2_sum_128_j, xx);

        const __m128 hx = _mm_mul_ps(h_k, x_k);
        e_128_j = _mm_add_ps(e_128_j, hx);

        h_p += 4;
        x_i += 4;
        RTC_DCHECK_GE(x.size(), x_i);
      }

      const int limit_mod_4 = limit - limit_by_4 * 4;
      if (limit_mod_4 > 0) {
        RTC_DCHECK_GE(x.size(), x_i);
        if (x_i == x.size()) {
          x_i = 0;
        }

        float* v = reinterpret_cast<float*>(&x2_sum_128_j);
        x2_sum[0] = v[0] + v[1] + v[2] + v[3];
        v = reinterpret_cast<float*>(&e_128_j);
        e[0] = v[0] + v[1] + v[2] + v[3];

        for (int k = 0; k < limit_mod_4; ++k) {
          float x_k = x[x_i++];
          x2_sum[0] += x_k * x_k;
          e[0] += *h_p++ * x_k;
          RTC_DCHECK_GE(x.size(), x_i);
        }

        x2_sum_128_j = _mm_set1_ps(x2_sum[0]);
        e_128_j = _mm_set1_ps(e[0]);
      }

      RTC_DCHECK_GE(x.size(), x_i);
      if (x_i == x.size()) {
        x_i = 0;
      }
    }

    for (size_t j = 0; j < num_filters - 1; ++j) {
      __m128 x2_sum_128_j_1 = _mm_set1_ps(0);
      __m128 e_128_j_1 = _mm_set1_ps(0);

      int chunk1 = std::min(filter_size_by_2, static_cast<int>(x.size() - x_i));
      int chunk2 = filter_size_by_2 - chunk1;

      for (int limit : {chunk1, chunk2}) {
        const int limit_by_4 = limit >> 2;
        for (int k = 0; k < limit_by_4; ++k) {
          const __m128 x_k = _mm_loadu_ps(&x[x_i]);
          const __m128 xx = _mm_mul_ps(x_k, x_k);
          x2_sum_128_j = _mm_add_ps(x2_sum_128_j, xx);
          x2_sum_128_j_1 = _mm_add_ps(x2_sum_128_j_1, xx);

          const __m128 h_k = _mm_loadu_ps(h_p);
          const __m128 h_k_1 = _mm_loadu_ps(h_p + 4);
          const __m128 hx = _mm_mul_ps(h_k, x_k);
          const __m128 hx_1 = _mm_mul_ps(h_k_1, x_k);
          e_128_j = _mm_add_ps(e_128_j, hx);
          e_128_j_1 = _mm_add_ps(e_128_j_1, hx_1);

          h_p += 8;
          x_i += 4;
          RTC_DCHECK_GE(x.size(), x_i);
        }

        const int limit_mod_4 = limit - limit_by_4 * 4;
        if (limit_mod_4 > 0) {
          RTC_DCHECK_GE(x.size(), x_i);
          if (x_i == x.size()) {
            x_i = 0;
          }

          float* v = reinterpret_cast<float*>(&x2_sum_128_j);
          x2_sum[j] = v[0] + v[1] + v[2] + v[3];
          v = reinterpret_cast<float*>(&x2_sum_128_j_1);
          x2_sum[j + 1] = v[0] + v[1] + v[2] + v[3];
          v = reinterpret_cast<float*>(&e_128_j);
          e[j] = v[0] + v[1] + v[2] + v[3];
          v = reinterpret_cast<float*>(&e_128_j_1);
          e[j + 1] = v[0] + v[1] + v[2] + v[3];

          for (int k = 0; k < limit_mod_4; ++k) {
            float x_k = x[x_i];
            float x2 = x_k * x_k;
            x2_sum[j] += x2;
            x2_sum[j + 1] += x2;
            e[j] += h_p[0] * x_k;
            e[j + 1] += h_p[4] * x_k;
            ++h_p;
            ++x_i;

            RTC_DCHECK_GE(x.size(), x_i);
          }

          x2_sum_128_j = _mm_set1_ps(x2_sum[j]);
          e_128_j = _mm_set1_ps(e[j]);
          x2_sum_128_j_1 = _mm_set1_ps(x2_sum[j + 1]);
          e_128_j_1 = _mm_set1_ps(e[j + 1]);
        }

        RTC_DCHECK_GE(x.size(), x_i);
        if (x_i == x.size()) {
          x_i = 0;
        }
      }

      float* v = reinterpret_cast<float*>(&x2_sum_128_j);
      x2_sum[j] = v[0] + v[1] + v[2] + v[3];
      v = reinterpret_cast<float*>(&e_128_j);
      e[j] = v[0] + v[1] + v[2] + v[3];

      x2_sum_128_j = x2_sum_128_j_1;
      e_128_j = e_128_j_1;
    }

    chunk1 = std::min(filter_size_by_2, static_cast<int>(x.size() - x_i));
    chunk2 = filter_size_by_2 - chunk1;

    for (int limit : {chunk1, chunk2}) {
      const int limit_by_4 = limit >> 2;
      for (int k = 0; k < limit_by_4; ++k) {
        const __m128 x_k = _mm_loadu_ps(&x[x_i]);
        const __m128 h_k = _mm_loadu_ps(h_p);
        const __m128 xx = _mm_mul_ps(x_k, x_k);
        x2_sum_128_j = _mm_add_ps(x2_sum_128_j, xx);

        const __m128 hx = _mm_mul_ps(h_k, x_k);
        e_128_j = _mm_add_ps(e_128_j, hx);

        h_p += 4;
        x_i += 4;
        RTC_DCHECK_GE(x.size(), x_i);
      }

      const int limit_mod_4 = limit - limit_by_4 * 4;
      if (limit_mod_4 > 0) {
        RTC_DCHECK_GE(x.size(), x_i);
        if (x_i == x.size()) {
          x_i = 0;
        }

        float* v = reinterpret_cast<float*>(&x2_sum_128_j);
        x2_sum[num_filters - 1] = v[0] + v[1] + v[2] + v[3];
        v = reinterpret_cast<float*>(&e_128_j);
        e[0] = v[0] + v[1] + v[2] + v[3];

        for (int k = 0; k < limit_mod_4; ++k) {
          float x_k = x[x_i++];
          x2_sum[num_filters - 1] += x_k * x_k;
          e[num_filters - 1] += *h_p++ * x_k;
          RTC_DCHECK_GE(x.size(), x_i);
        }

        x2_sum_128_j = _mm_set1_ps(x2_sum[num_filters - 1]);
        e_128_j = _mm_set1_ps(e[num_filters - 1]);
      }

      RTC_DCHECK_GE(x.size(), x_i);
      if (x_i == x.size()) {
        x_i = 0;
      }
    }

    float* v = reinterpret_cast<float*>(&x2_sum_128_j);
    x2_sum[num_filters - 1] = v[0] + v[1] + v[2] + v[3];
    v = reinterpret_cast<float*>(&e_128_j);
    e[num_filters - 1] = v[0] + v[1] + v[2] + v[3];

    const bool saturation =
        y[y.size() - 1 - i] >= 32000.f || y[y.size() - 1 - i] <= -32000.f;
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

    h_p = &h[0];
    x_i = x0;

    __m128 alpha_128_j = _mm_set1_ps(e[0]);

    chunk1 = std::min(filter_size_by_2, static_cast<int>(x.size() - x_i));
    chunk2 = filter_size_by_2 - chunk1;

    for (int limit : {chunk1, chunk2}) {
      const int limit_by_4 = limit >> 2;
      for (int k = 0; k < limit_by_4; ++k) {
        const __m128 x_k = _mm_loadu_ps(&x[x_i]);

        __m128 alpha_x = _mm_mul_ps(alpha_128_j, x_k);
        __m128 h_k = _mm_loadu_ps(h_p);
        h_k = _mm_add_ps(h_k, alpha_x);
        _mm_storeu_ps(h_p, h_k);

        h_p += 4;
        x_i += 4;
        RTC_DCHECK_GE(x.size(), x_i);
      }

      const int limit_mod_4 = limit - limit_by_4 * 4;
      if (limit_mod_4 > 0) {
        RTC_DCHECK_GE(x.size(), x_i);
        if (x_i == x.size()) {
          x_i = 0;
        }

        for (int k = 0; k < limit_mod_4; ++k) {
          *h_p++ += e[0] * x[x_i++];
          RTC_DCHECK_GE(x.size(), x_i);
        }
      }

      RTC_DCHECK_GE(x.size(), x_i);
      if (x_i == x.size()) {
        x_i = 0;
      }
    }

    for (size_t j = 0; j < num_filters - 1; ++j) {
      __m128 alpha_128_j_1 = _mm_set1_ps(e[j + 1]);

      chunk1 = std::min(filter_size_by_2, static_cast<int>(x.size() - x_i));
      chunk2 = filter_size_by_2 - chunk1;

      for (int limit : {chunk1, chunk2}) {
        const int limit_by_4 = limit >> 2;
        for (int k = 0; k < limit_by_4; ++k) {
          const __m128 x_k = _mm_loadu_ps(&x[x_i]);

          __m128 alpha_x = _mm_mul_ps(alpha_128_j, x_k);
          __m128 h_k = _mm_loadu_ps(h_p);
          h_k = _mm_add_ps(h_k, alpha_x);
          _mm_storeu_ps(h_p, h_k);
          h_p += 4;

          alpha_x = _mm_mul_ps(alpha_128_j_1, x_k);
          h_k = _mm_loadu_ps(h_p);
          h_k = _mm_add_ps(h_k, alpha_x);
          _mm_storeu_ps(h_p, h_k);

          h_p += 4;
          x_i += 4;
          RTC_DCHECK_GE(x.size(), x_i);
        }

        const int limit_mod_4 = limit - limit_by_4 * 4;
        if (limit_mod_4 > 0) {
          RTC_DCHECK_GE(x.size(), x_i);
          if (x_i == x.size()) {
            x_i = 0;
          }

          for (int k = 0; k < limit_mod_4; ++k) {
            float x_k = x[x_i];
            h_p[0] += e[j] * x_k;
            h_p[4] += e[j + 1] * x_k;
            ++h_p;
            ++x_i;
            RTC_DCHECK_GE(x.size(), x_i);
          }
        }

        RTC_DCHECK_GE(x.size(), x_i);
        if (x_i == x.size()) {
          x_i = 0;
        }
      }

      alpha_128_j = alpha_128_j_1;
    }

    chunk1 = std::min(filter_size_by_2, static_cast<int>(x.size() - x_i));
    chunk2 = filter_size_by_2 - chunk1;

    for (int limit : {chunk1, chunk2}) {
      const int limit_by_4 = limit >> 2;
      for (int k = 0; k < limit_by_4; ++k) {
        const __m128 x_k = _mm_loadu_ps(&x[x_i]);

        __m128 alpha_x = _mm_mul_ps(alpha_128_j, x_k);
        __m128 h_k = _mm_loadu_ps(h_p);
        h_k = _mm_add_ps(h_k, alpha_x);
        _mm_storeu_ps(h_p, h_k);

        h_p += 4;
        x_i += 4;
        RTC_DCHECK_GE(x.size(), x_i);
      }

      const int limit_mod_4 = limit - limit_by_4 * 4;
      if (limit_mod_4 > 0) {
        RTC_DCHECK_GE(x.size(), x_i);
        if (x_i == x.size()) {
          x_i = 0;
        }

        for (int k = 0; k < limit_mod_4; ++k) {
          *h_p++ += e[num_filters - 1] * x[x_i++];
          RTC_DCHECK_GE(x.size(), x_i);
        }
      }

      RTC_DCHECK_GE(x.size(), x_i);
      if (x_i == x.size()) {
        x_i = 0;
      }
    }

    if (++x0 == x.size()) {
      x0 = 0;
    }
  }
}

void FilterSymmetricOverlap(rtc::ArrayView<const float> x,
                            size_t x_start,
                            rtc::ArrayView<const float> y,
                            size_t filter_size,
                            size_t num_filters,
                            float x2_sum_threshold,
                            float step_size,
                            rtc::ArrayView<float> x2_sum,
                            rtc::ArrayView<float> e,
                            rtc::ArrayView<float> e2_sum,
                            std::vector<bool>* filters_updated,
                            rtc::ArrayView<float> h) {
  RTC_DCHECK_EQ(0, h.size() % 4);
  RTC_DCHECK_EQ(0, x.size() % 4);
  RTC_DCHECK_EQ(0, x_start % 4);
  RTC_DCHECK_EQ(num_filters, x2_sum.size());
  RTC_DCHECK_EQ(num_filters, e2_sum.size());
  RTC_DCHECK_EQ(num_filters, e.size());
  RTC_DCHECK_EQ(num_filters, filters_updated->size());

  size_t x0 = x_start;

  int filter_size_by_2 = static_cast<int>(filter_size >> 1);

  std::fill(e2_sum.begin(), e2_sum.end(), 0.f);
  std::fill(filters_updated->begin(), filters_updated->end(), false);
  for (size_t i = 0; i < y.size(); ++i) {
    std::fill(x2_sum.begin(), x2_sum.end(), 0.f);
    std::fill(e.begin(), e.end(), 0.f);

    size_t x_i = x0;
    float* h_p = &h[0];

    int chunk1 = std::min(filter_size_by_2, static_cast<int>(x.size() - x_i));
    int chunk2 = filter_size_by_2 - chunk1;

    for (int limit : {chunk1, chunk2}) {
      const int limit_by_4 = limit >> 2;
      for (int k = 0; k < limit_by_4; ++k) {
        for (size_t n = 0; n < 4; ++n) {
          x2_sum[0] += x[x_i + n] * x[x_i + n];
          e[0] += h_p[n] * x[x_i + n];
        }
        h_p += 4;
        x_i += 4;
        RTC_DCHECK_GE(x.size(), x_i);
      }

      const int limit_mod_4 = limit - limit_by_4 * 4;
      if (limit_mod_4 > 0) {
        RTC_DCHECK_GE(x.size(), x_i);
        if (x_i == x.size()) {
          x_i = 0;
        }

        for (int k = 0; k < limit_mod_4; ++k) {
          x2_sum[0] += x[x_i] * x[x_i];
          e[0] += h_p[0] * x[x_i];
          ++h_p;
          ++x_i;
          RTC_DCHECK_GE(x.size(), x_i);
        }
      }

      RTC_DCHECK_GE(x.size(), x_i);
      if (x_i == x.size()) {
        x_i = 0;
      }
    }

    for (size_t j = 0; j < num_filters - 1; ++j) {
      chunk1 = std::min(filter_size_by_2, static_cast<int>(x.size() - x_i));
      chunk2 = filter_size_by_2 - chunk1;

      for (int limit : {chunk1, chunk2}) {
        const int limit_by_4 = limit >> 2;
        for (int k = 0; k < limit_by_4; ++k) {
          for (size_t n = 0; n < 4; ++n) {
            float x2 = x[x_i + n] * x[x_i + n];
            x2_sum[j] += x2;
            x2_sum[j + 1] += x2;
            e[j] += h_p[n] * x[x_i + n];
            e[j + 1] += h_p[4 + n] * x[x_i + n];
          }
          h_p += 8;
          x_i += 4;
          RTC_DCHECK_GE(x.size(), x_i);
        }

        const int limit_mod_4 = limit - limit_by_4 * 4;
        if (limit_mod_4 > 0) {
          RTC_DCHECK_GE(x.size(), x_i);
          if (x_i == x.size()) {
            x_i = 0;
          }

          for (int k = 0; k < limit_mod_4; ++k) {
            float x2 = x[x_i] * x[x_i];
            x2_sum[j] += x2;
            x2_sum[j + 1] += x2;
            e[j] += h_p[0] * x[x_i];
            e[j + 1] += h_p[4] * x[x_i];
            ++h_p;
            ++x_i;
            RTC_DCHECK_GE(x.size(), x_i);
          }
        }

        RTC_DCHECK_GE(x.size(), x_i);
        if (x_i == x.size()) {
          x_i = 0;
        }
      }
    }

    chunk1 = std::min(filter_size_by_2, static_cast<int>(x.size() - x_i));
    chunk2 = filter_size_by_2 - chunk1;

    for (int limit : {chunk1, chunk2}) {
      const int limit_by_4 = limit >> 2;
      for (int k = 0; k < limit_by_4; ++k) {
        for (size_t n = 0; n < 4; ++n) {
          x2_sum[num_filters - 1] += x[x_i + n] * x[x_i + n];
          e[num_filters - 1] += h_p[n] * x[x_i + n];
        }
        h_p += 4;
        x_i += 4;
        RTC_DCHECK_GE(x.size(), x_i);
      }

      const int limit_mod_4 = limit - limit_by_4 * 4;
      if (limit_mod_4 > 0) {
        RTC_DCHECK_GE(x.size(), x_i);
        if (x_i == x.size()) {
          x_i = 0;
        }

        for (int k = 0; k < limit_mod_4; ++k) {
          x2_sum[num_filters - 1] += x[x_i] * x[x_i];
          e[num_filters - 1] += h_p[0] * x[x_i];
          ++h_p;
          ++x_i;
          RTC_DCHECK_GE(x.size(), x_i);
        }
      }

      RTC_DCHECK_GE(x.size(), x_i);
      if (x_i == x.size()) {
        x_i = 0;
      }
    }

    const bool saturation =
        y[y.size() - 1 - i] >= 32000.f || y[y.size() - 1 - i] <= -32000.f;
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
    h_p = &h[0];

    chunk1 = std::min(filter_size_by_2, static_cast<int>(x.size() - x_i));
    chunk2 = filter_size_by_2 - chunk1;

    for (int limit : {chunk1, chunk2}) {
      const int limit_by_4 = limit >> 2;
      for (int k = 0; k < limit_by_4; ++k) {
        for (size_t n = 0; n < 4; ++n) {
          h_p[n] += e[0] * x[x_i + n];
        }
        h_p += 4;
        x_i += 4;
      }

      const int limit_mod_4 = limit - limit_by_4 * 4;
      if (limit_mod_4 > 0) {
        RTC_DCHECK_GE(x.size(), x_i);
        if (x_i == x.size()) {
          x_i = 0;
        }

        for (int k = 0; k < limit_mod_4; ++k) {
          h_p[0] += e[0] * x[x_i];
          ++h_p;
          ++x_i;
          RTC_DCHECK_GE(x.size(), x_i);
        }
      }

      RTC_DCHECK_GE(x.size(), x_i);
      if (x_i == x.size()) {
        x_i = 0;
      }
    }

    for (size_t j = 0; j < num_filters - 1; ++j) {
      chunk1 = std::min(filter_size_by_2, static_cast<int>(x.size() - x_i));
      chunk2 = filter_size_by_2 - chunk1;

      for (int limit : {chunk1, chunk2}) {
        const int limit_by_4 = limit >> 2;
        for (int k = 0; k < limit_by_4; ++k) {
          for (size_t n = 0; n < 4; ++n) {
            h_p[n] += e[j] * x[x_i + n];
            h_p[4 + n] += e[j + 1] * x[x_i + n];
          }
          h_p += 8;
          x_i += 4;
          RTC_DCHECK_GE(x.size(), x_i);
        }

        const int limit_mod_4 = limit - limit_by_4 * 4;
        if (limit_mod_4 > 0) {
          RTC_DCHECK_GE(x.size(), x_i);
          if (x_i == x.size()) {
            x_i = 0;
          }

          for (int k = 0; k < limit_mod_4; ++k) {
            h_p[0] += e[j] * x[x_i];
            h_p[4] += e[j + 1] * x[x_i];
            ++h_p;
            ++x_i;
            RTC_DCHECK_GE(x.size(), x_i);
          }
        }

        RTC_DCHECK_GE(x.size(), x_i);
        if (x_i == x.size()) {
          x_i = 0;
        }
      }
    }

    chunk1 = std::min(filter_size_by_2, static_cast<int>(x.size() - x_i));
    chunk2 = filter_size_by_2 - chunk1;

    for (int limit : {chunk1, chunk2}) {
      const int limit_by_4 = limit >> 2;
      for (int k = 0; k < limit_by_4; ++k) {
        for (size_t n = 0; n < 4; ++n) {
          h_p[n] += e[num_filters - 1] * x[x_i + n];
        }
        h_p += 4;
        x_i += 4;
        RTC_DCHECK_GE(x.size(), x_i);
      }

      const int limit_mod_4 = limit - limit_by_4 * 4;
      if (limit_mod_4 > 0) {
        RTC_DCHECK_GE(x.size(), x_i);
        if (x_i == x.size()) {
          x_i = 0;
        }

        for (int k = 0; k < limit_mod_4; ++k) {
          h_p[0] += e[num_filters - 1] * x[x_i];
          ++h_p;
          ++x_i;
          RTC_DCHECK_GE(x.size(), x_i);
        }
      }

      RTC_DCHECK_GE(x.size(), x_i);
      if (x_i == x.size()) {
        x_i = 0;
      }
    }

    if (++x0 == x.size()) {
      x0 = 0;
    }
  }
}

void FindPeaksSymmetricOverlap(rtc::ArrayView<const float> h,
                               size_t num_filters,
                               rtc::ArrayView<size_t> peaks) {
  RTC_DCHECK_EQ(num_filters, peaks.size());

  const float* h_p = &h[0];

  float m0 = 0.f;
  size_t p0 = 0;
  for (size_t k = 0; k < 64; ++k) {
    float tmp = fabs(*h_p++);
    if (tmp > m0) {
      m0 = tmp;
      p0 = k;
    }
  }

  for (size_t j = 0; j < num_filters - 1; ++j) {
    float m1 = 0.f;
    size_t p1 = 0;
    for (size_t k = 0, i = 0; k < 16; ++k, i += 4) {
      for (size_t n = 0; n < 4; ++n) {
        float tmp = fabs(*h_p++);
        if (tmp > m0) {
          m0 = tmp;
          p0 = i + n + 64;
        }
      }
      for (size_t n = 0; n < 4; ++n) {
        float tmp = fabs(*h_p++);
        if (tmp > m1) {
          m1 = tmp;
          p1 = i + n;
        }
      }
    }
    peaks[j] = p0;
    m0 = m1;
    p0 = p1;
  }

  for (size_t k = 0; k < 64; ++k) {
    float tmp = fabs(*h_p++);
    if (tmp > m0) {
      m0 = tmp;
      p0 = k + 64;
    }
  }
  peaks[num_filters - 1] = p0;
}

}  // namespace aec3

namespace {
float GetEstimatorSmoothing() {
  return 0.4f;
}
}  // namespace

MatchedFilter::MatchedFilter(ApmDataDumper* data_dumper,
                             Aec3Optimization optimization,
                             size_t sub_block_size,
                             size_t window_size_sub_blocks,
                             int num_matched_filters,
                             size_t alignment_shift_sub_blocks,
                             float excitation_limit)
    : data_dumper_(data_dumper),
      optimization_(optimization),
      sub_block_size_(sub_block_size),
      filter_intra_lag_shift_(alignment_shift_sub_blocks * sub_block_size_),
      symmetric_overlap_(2 * alignment_shift_sub_blocks ==
                         window_size_sub_blocks),
      filter_size_(window_size_sub_blocks * sub_block_size_),
      num_filters_(num_matched_filters),
      filters_generic_(symmetric_overlap_ ? 0 : num_matched_filters,
                       std::vector<float>(filter_size_, 0.f)),
      lag_estimates_(num_matched_filters),
      filters_offsets_(symmetric_overlap_ ? 0 : num_matched_filters, 0),
      x2_threshold_(excitation_limit * excitation_limit *
                    window_size_sub_blocks * sub_block_size_),
      estimator_smoothing_(GetEstimatorSmoothing()) {
  RTC_DCHECK(data_dumper);
  RTC_DCHECK_LT(0, window_size_sub_blocks);
  RTC_DCHECK((kBlockSize % sub_block_size) == 0);
  RTC_DCHECK((sub_block_size % 4) == 0);

  if (symmetric_overlap_) {
    x2_sum_.resize(num_filters_);
    std::fill(x2_sum_.begin(), x2_sum_.end(), 0.f);

    e_.resize(num_filters_);
    std::fill(e_.begin(), e_.end(), 0.f);

    e2_sum_.resize(num_filters_);
    std::fill(e2_sum_.begin(), e2_sum_.end(), 0.f);

    peaks_.resize(num_filters_);
    std::fill(peaks_.begin(), peaks_.end(), 0);

    filters_updated_.resize(num_filters_);
    std::fill(filters_updated_.begin(), filters_updated_.end(), true);

    filters_symmetric_overlap_.resize(num_filters_ * filter_size_);
    std::fill(filters_symmetric_overlap_.begin(),
              filters_symmetric_overlap_.end(), 0.f);
  }
}

MatchedFilter::~MatchedFilter() = default;

void MatchedFilter::Reset() {
  if (symmetric_overlap_) {
    std::fill(filters_symmetric_overlap_.begin(),
              filters_symmetric_overlap_.end(), 0.f);
  } else {
    for (auto& f : filters_generic_) {
      std::fill(f.begin(), f.end(), 0.f);
    }
  }

  for (auto& l : lag_estimates_) {
    l = MatchedFilter::LagEstimate();
  }
}

void MatchedFilter::Update(const DownsampledRenderBuffer& render_buffer,
                           rtc::ArrayView<const float> capture) {
  if (symmetric_overlap_) {
    UpdateSymmetricOverlap(render_buffer, capture);
  } else {
    UpdateGeneric(render_buffer, capture);
  }
}

void MatchedFilter::UpdateSymmetricOverlap(
    const DownsampledRenderBuffer& render_buffer,
    rtc::ArrayView<const float> capture) {
  RTC_DCHECK_EQ(sub_block_size_, capture.size());
  auto& y = capture;

  switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
    case Aec3Optimization::kSse2:
      aec3::FilterSymmetricOverlap_SSE2(
          render_buffer.buffer, render_buffer.read, capture, filter_size_,
          num_filters_, x2_threshold_, estimator_smoothing_, x2_sum_, e_,
          e2_sum_, &filters_updated_, filters_symmetric_overlap_);
      break;
#endif
#if defined(WEBRTC_HAS_NEON)
    case Aec3Optimization::kNeon:
      aec3::FilterSymmetricOverlap_NEON(
          render_buffer.buffer, render_buffer.read, capture, filter_size_,
          num_filters_, x2_threshold_, estimator_smoothing_, x2_sum_, e_,
          e2_sum_, &filters_updated_, filters_symmetric_overlap_);
      break;
#endif
    default:
      aec3::FilterSymmetricOverlap(
          render_buffer.buffer, render_buffer.read, capture, filter_size_,
          num_filters_, x2_threshold_, estimator_smoothing_, x2_sum_, e_,
          e2_sum_, &filters_updated_, filters_symmetric_overlap_);
  }

  aec3::FindPeaksSymmetricOverlap(filters_symmetric_overlap_, num_filters_,
                                  peaks_);

  // Compute anchor for the matched filter error.
  const float error_sum_anchor =
      std::inner_product(y.begin(), y.end(), y.begin(), 0.f);

  size_t lag_estimate_bound = filter_size_ - 10;
  size_t intra_filter_shift = filter_size_ >> 1;
  const float kMatchingFilterThreshold = 0.2f;
  for (size_t j = 0; j < num_filters_; ++j) {
    // Update the lag estimates for the matched filter.
    lag_estimates_[j] =
        LagEstimate(error_sum_anchor - e2_sum_[j],
                    (peaks_[j] > 2 && peaks_[j] < lag_estimate_bound &&
                     e2_sum_[j] < kMatchingFilterThreshold * error_sum_anchor),
                    peaks_[j] + j * intra_filter_shift, filters_updated_[j]);
  }
  data_dumper_->DumpRaw("aec3_correlator_h_symmetric_overlap",
                        filters_symmetric_overlap_);
}

void MatchedFilter::UpdateGeneric(const DownsampledRenderBuffer& render_buffer,
                                  rtc::ArrayView<const float> capture) {
  RTC_DCHECK_EQ(sub_block_size_, capture.size());
  auto& y = capture;

  // Apply all matched filters.
  size_t alignment_shift = 0;
  for (size_t n = 0; n < filters_generic_.size(); ++n) {
    float error_sum = 0.f;
    bool filters_updated = false;

    size_t x_start_index =
        (render_buffer.read + alignment_shift + sub_block_size_ - 1) %
        render_buffer.buffer.size();

    switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
      case Aec3Optimization::kSse2:
        aec3::MatchedFilterCore_SSE2(x_start_index, x2_threshold_,
                                     estimator_smoothing_, render_buffer.buffer,
                                     y, filters_generic_[n], &filters_updated,
                                     &error_sum);
        break;
#endif
#if defined(WEBRTC_HAS_NEON)
      case Aec3Optimization::kNeon:
        aec3::MatchedFilterCore_NEON(
            x_start_index, x2_threshold_, estimator_smoothing_,
            render_buffer.buffer, y, filters_[n], &filters_updated, &error_sum);
        break;
#endif
      default:
        aec3::MatchedFilterCore(x_start_index, x2_threshold_,
                                estimator_smoothing_, render_buffer.buffer, y,
                                filters_generic_[n], &filters_updated,
                                &error_sum);
    }

    // Compute anchor for the matched filter error.
    const float error_sum_anchor =
        std::inner_product(y.begin(), y.end(), y.begin(), 0.f);

    // Estimate the lag in the matched filter as the distance to the portion in
    // the filter that contributes the most to the matched filter output. This
    // is detected as the peak of the matched filter.
    const size_t lag_estimate = std::distance(
        filters_generic_[n].begin(),
        std::max_element(
            filters_generic_[n].begin(), filters_generic_[n].end(),
            [](float a, float b) -> bool { return a * a < b * b; }));

    // Update the lag estimates for the matched filter.
    const float kMatchingFilterThreshold = 0.2f;
    lag_estimates_[n] = LagEstimate(
        error_sum_anchor - error_sum,
        (lag_estimate > 2 && lag_estimate < (filters_generic_[n].size() - 10) &&
         error_sum < kMatchingFilterThreshold * error_sum_anchor),
        lag_estimate + alignment_shift, filters_updated);

    RTC_DCHECK_GE(10, filters_generic_.size());
    switch (n) {
      case 0:
        data_dumper_->DumpRaw("aec3_correlator_0_h", filters_generic_[0]);
        break;
      case 1:
        data_dumper_->DumpRaw("aec3_correlator_1_h", filters_generic_[1]);
        break;
      case 2:
        data_dumper_->DumpRaw("aec3_correlator_2_h", filters_generic_[2]);
        break;
      case 3:
        data_dumper_->DumpRaw("aec3_correlator_3_h", filters_generic_[3]);
        break;
      case 4:
        data_dumper_->DumpRaw("aec3_correlator_4_h", filters_generic_[4]);
        break;
      case 5:
        data_dumper_->DumpRaw("aec3_correlator_5_h", filters_generic_[5]);
        break;
      case 6:
        data_dumper_->DumpRaw("aec3_correlator_6_h", filters_generic_[6]);
        break;
      case 7:
        data_dumper_->DumpRaw("aec3_correlator_7_h", filters_generic_[7]);
        break;
      case 8:
        data_dumper_->DumpRaw("aec3_correlator_8_h", filters_generic_[8]);
        break;
      case 9:
        data_dumper_->DumpRaw("aec3_correlator_9_h", filters_generic_[9]);
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
  for (size_t k = 0; k < num_filters_; ++k) {
    int start = static_cast<int>(alignment_shift * downsampling_factor);
    int end = static_cast<int>((alignment_shift + filter_size_) *
                               downsampling_factor);
    RTC_LOG(LS_INFO) << "Filter " << k << ": start: "
                     << (start - static_cast<int>(shift)) / fs_by_1000
                     << " ms, end: "
                     << (end - static_cast<int>(shift)) / fs_by_1000 << " ms.";
    alignment_shift += filter_intra_lag_shift_;
  }
}

}  // namespace webrtc
