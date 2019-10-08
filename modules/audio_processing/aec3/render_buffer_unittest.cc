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
#include <vector>

#include "rtc_base/system/arch.h"
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include "system_wrappers/include/cpu_features_wrapper.h"
#include "test/gtest.h"

namespace webrtc {
namespace aec3 {
namespace {
void PopulateSpectrumBuffer(size_t num_partitions,
                            size_t num_render_channels,
                            SpectrumBuffer* spectrum_buffer) {
  for (size_t p = 0; p < num_partitions; ++p) {
    for (size_t ch = 0; p < num_render_channels; ++p) {
      for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
        spectrum_buffer->buffer[p][ch][k] = (209 * p + 17 * ch + k) % 1000;
      }
    }
  }
  spectrum_buffer->read = 9;
}

}  // namespace

#if defined(WEBRTC_HAS_NEON)
TEST(RenderBuffer, ComputeSpectralSumNeonOptimizations) {
  constexpr size_t kNumBands = 3;
  constexpr size_t kNumPartitions = 10;
  for (size_t num_render_channels : {1, 2, 4, 8}) {
    FftBuffer fft_buffer(kNumPartitions, num_render_channels);
    BlockBuffer block_buffer(kNumPartitions, kNumBands, num_render_channels,
                             kBlockSize);
    SpectrumBuffer spectrum_buffer(kNumPartitions, num_render_channels,
                                   kFftLengthBy2Plus1);
    RenderBuffer render_buffer(&block_buffer, &spectrum_buffer, &fft_buffer);
    PopulateSpectrumBuffer(kNumPartitions, num_render_channels,
                           &spectrum_buffer);

    std::array<float, kFftLengthBy2Plus1> X2_C;
    std::array<float, kFftLengthBy2Plus1> X2_Neon;
    aec3::ComputeSpectralSum(spectrum_buffer, 9, &X2_C);
    aec3::ComputeSpectralSum_Neon(spectrum_buffer, 9, &X2_Neon);

    for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
      EXPECT_FLOAT_EQ(X2_C[k], X2_Neon[k]);
    }
  }
}

TEST(RenderBuffer, ComputeSpectralSumsNeonOptimizations) {
  constexpr size_t kNumBands = 3;
  constexpr size_t kNumPartitions = 10;
  for (size_t num_render_channels : {1, 2, 4, 8}) {
    FftBuffer fft_buffer(kNumPartitions, num_render_channels);
    BlockBuffer block_buffer(kNumPartitions, kNumBands, num_render_channels,
                             kBlockSize);
    SpectrumBuffer spectrum_buffer(kNumPartitions, num_render_channels,
                                   kFftLengthBy2Plus1);
    RenderBuffer render_buffer(&block_buffer, &spectrum_buffer, &fft_buffer);
    PopulateSpectrumBuffer(kNumPartitions, num_render_channels,
                           &spectrum_buffer);

    std::array<float, kFftLengthBy2Plus1> X2_shorter_C;
    std::array<float, kFftLengthBy2Plus1> X2_shorter_Neon;
    std::array<float, kFftLengthBy2Plus1> X2_longer_C;
    std::array<float, kFftLengthBy2Plus1> X2_longer_Neon;
    aec3::ComputeSpectralSums(spectrum_buffer, 7, 9, &X2_shorter_C,
                              &X2_longer_C);
    aec3::ComputeSpectralSums_Neon(spectrum_buffer, 7, 9, &X2_shorter_Neon,
                                   &X2_longer_Neon);

    for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
      EXPECT_FLOAT_EQ(X2_shorter_C[k], X2_shorter_Neon[k]);
    }
    for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
      EXPECT_FLOAT_EQ(X2_longer_C[k], X2_longer_Neon[k]);
    }
  }
}

#endif

#if defined(WEBRTC_ARCH_X86_FAMILY)
TEST(RenderBuffer, ComputeSpectralSumSse2Optimizations) {
  bool use_sse2 = (WebRtc_GetCPUInfo(kSSE2) != 0);
  if (use_sse2) {
    constexpr size_t kNumBands = 3;
    constexpr size_t kNumPartitions = 10;
    for (size_t num_render_channels : {1, 2, 4, 8}) {
      FftBuffer fft_buffer(kNumPartitions, num_render_channels);
      BlockBuffer block_buffer(kNumPartitions, kNumBands, num_render_channels,
                               kBlockSize);
      SpectrumBuffer spectrum_buffer(kNumPartitions, num_render_channels,
                                     kFftLengthBy2Plus1);
      RenderBuffer render_buffer(&block_buffer, &spectrum_buffer, &fft_buffer);
      PopulateSpectrumBuffer(kNumPartitions, num_render_channels,
                             &spectrum_buffer);

      std::array<float, kFftLengthBy2Plus1> X2_C;
      std::array<float, kFftLengthBy2Plus1> X2_Sse2;
      aec3::ComputeSpectralSum(spectrum_buffer, 9, &X2_C);
      aec3::ComputeSpectralSum_Sse2(spectrum_buffer, 9, &X2_Sse2);

      for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
        EXPECT_FLOAT_EQ(X2_C[k], X2_Sse2[k]);
      }
    }
  }
}

TEST(RenderBuffer, ComputeSpectralSumsSse2Optimizations) {
  bool use_sse2 = (WebRtc_GetCPUInfo(kSSE2) != 0);
  if (use_sse2) {
    constexpr size_t kNumBands = 3;
    constexpr size_t kNumPartitions = 10;
    for (size_t num_render_channels : {1, 2, 4, 8}) {
      FftBuffer fft_buffer(kNumPartitions, num_render_channels);
      BlockBuffer block_buffer(kNumPartitions, kNumBands, num_render_channels,
                               kBlockSize);
      SpectrumBuffer spectrum_buffer(kNumPartitions, num_render_channels,
                                     kFftLengthBy2Plus1);
      RenderBuffer render_buffer(&block_buffer, &spectrum_buffer, &fft_buffer);
      PopulateSpectrumBuffer(kNumPartitions, num_render_channels,
                             &spectrum_buffer);

      std::array<float, kFftLengthBy2Plus1> X2_shorter_C;
      std::array<float, kFftLengthBy2Plus1> X2_shorter_Sse2;
      std::array<float, kFftLengthBy2Plus1> X2_longer_C;
      std::array<float, kFftLengthBy2Plus1> X2_longer_Sse2;
      aec3::ComputeSpectralSums(spectrum_buffer, 7, 9, &X2_shorter_C,
                                &X2_longer_C);
      aec3::ComputeSpectralSums_Sse2(spectrum_buffer, 7, 9, &X2_shorter_Sse2,
                                     &X2_longer_Sse2);

      for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
        EXPECT_FLOAT_EQ(X2_shorter_C[k], X2_shorter_Sse2[k]);
      }
      for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
        EXPECT_FLOAT_EQ(X2_longer_C[k], X2_longer_Sse2[k]);
      }
    }
  }
}
#endif

}  // namespace aec3

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for non-null fft buffer.
TEST(RenderBuffer, NullExternalFftBuffer) {
  BlockBuffer block_buffer(10, 3, 1, kBlockSize);
  SpectrumBuffer spectrum_buffer(10, 1, kFftLengthBy2Plus1);
  EXPECT_DEATH(RenderBuffer(&block_buffer, &spectrum_buffer, nullptr), "");
}

// Verifies the check for non-null spectrum buffer.
TEST(RenderBuffer, NullExternalSpectrumBuffer) {
  FftBuffer fft_buffer(10, 1);
  BlockBuffer block_buffer(10, 3, 1, kBlockSize);
  EXPECT_DEATH(RenderBuffer(&block_buffer, nullptr, &fft_buffer), "");
}

// Verifies the check for non-null block buffer.
TEST(RenderBuffer, NullExternalBlockBuffer) {
  FftBuffer fft_buffer(10, 1);
  SpectrumBuffer spectrum_buffer(10, 1, kFftLengthBy2Plus1);
  EXPECT_DEATH(RenderBuffer(nullptr, &spectrum_buffer, &fft_buffer), "");
}

#endif

}  // namespace webrtc
