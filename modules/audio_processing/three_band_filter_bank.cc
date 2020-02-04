/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// An implementation of a 3-band FIR filter-bank with DCT modulation, similar to
// the proposed in "Multirate Signal Processing for Communication Systems" by
// Fredric J Harris.
//
// The idea is to take a heterodyne system and change the order of the
// components to get something which is efficient to implement digitally.
//
// It is possible to separate the filter using the noble identity as follows:
//
// H(z) = H0(z^3) + z^-1 * H1(z^3) + z^-2 * H2(z^3)
//
// This is used in the analysis stage to first downsample serial to parallel
// and then filter each branch with one of these polyphase decompositions of the
// lowpass prototype. Because each filter is only a modulation of the prototype,
// it is enough to multiply each coefficient by the respective cosine value to
// shift it to the desired band. But because the cosine period is 12 samples,
// it requires separating the prototype even further using the noble identity.
// After filtering and modulating for each band, the output of all filters is
// accumulated to get the downsampled bands.
//
// A similar logic can be applied to the synthesis stage.

#include "modules/audio_processing/three_band_filter_bank.h"

#include <array>

#include "api/array_view.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// Factors to take into account when choosing |kNumCoeffs|:
//   1. Higher |kNumCoeffs|, means faster transition, which ensures less
//      aliasing. This is especially important when there is non-linear
//      processing between the splitting and merging.
//   2. The delay that this filter bank introduces is
//      |kNumBands| * |kSparsity| * |kNumCoeffs| / 2, so it increases linearly
//      with |kNumCoeffs|.
//   3. The computation complexity also increases linearly with |kNumCoeffs|.

// The Matlab code to generate these |kFilterCoeffs| is:
//
// N = kNumBands * kSparsity * kNumCoeffs - 1;
// h = fir1(N, 1 / (2 * kNumBands), kaiser(N + 1, 3.5));
// reshape(h, kNumBands * kSparsity, kNumCoeffs);
//
// The code below uses:
// kNumCoeffs = 4;
// kNumBands = 3;
// kSparsity = 4;

// Because the total bandwidth of the lower and higher band is double the middle
// one (because of the spectrum parity), the low-pass prototype is half the
// bandwidth of 1 / (2 * |3|) and is then shifted with cosine modulation
// to the right places.
// A Kaiser window is used because of its flexibility and the alpha is set to
// 3.5, since that sets a stop band attenuation of 40dB ensuring a fast
// transition.

constexpr int kNumBands = 3;
constexpr int kSubSampling = 3;
constexpr int kStride = 4;
constexpr int kStrideLog2 = 2;
constexpr int kFullBandSize = 480;
constexpr int kSplitBandSize = kFullBandSize / kNumBands;
constexpr int kDctSize = 3;
constexpr int kFilterSize = 4;
constexpr int kNumFilters = 10;
constexpr int kMemorySize = 15;

const float kFilterCoeffs[kNumFilters][kFilterSize] = {
    {-0.00047749f, -0.00496888f, +0.16547118f, +0.00425496f},
    {-0.00173287f, -0.01585778f, +0.14989004f, +0.00994113f},
    {-0.00304815f, -0.02536082f, +0.12154542f, +0.01157993f},
    {-0.00346946f, -0.02587886f, +0.04760441f, +0.00607594f},
    {-0.00154717f, -0.01136076f, +0.01387458f, +0.00186353f},
    {+0.00186353f, +0.01387458f, -0.01136076f, -0.00154717f},
    {+0.00607594f, +0.04760441f, -0.02587886f, -0.00346946f},
    {+0.00983212f, +0.08543175f, -0.02982767f, -0.00383509f},
    {+0.00994113f, +0.14989004f, -0.01585778f, -0.00173287f},
    {+0.00425496f, +0.16547118f, -0.00496888f, -0.00047749f}};

constexpr int kZeroFilterIndex1 = 3;
constexpr int kZeroFilterIndex2 = 9;

const float kDctModulation[kNumFilters][kDctSize] = {
    {2.f, 2.f, 2.f},
    {1.73205077f, 0, -1.73205077f},
    {1.f, -2.f, 1.f},
    {-1.f, 2.f, -1.f},
    {-1.73205077f, 0, 1.73205077f},
    {-2.f, -2.f, -2.f},
    {-1.73205077f, 0, 1.73205077f},
    {-1.f, 2.f, -1.f},
    {1.f, -2.f, 1.f},
    {1.73205077f, 0, -1.73205077f}};

void FilterCore(int shift_start,
                rtc::ArrayView<const float, kFilterSize> filter,
                rtc::ArrayView<const float, kSplitBandSize> in,
                rtc::ArrayView<float, kSplitBandSize> out,
                rtc::ArrayView<float, kMemorySize> state) {
  std::fill(out.begin(), out.end(), 0.f);

  int k = 0;
  int shift = shift_start;
  for (; k < shift_start; ++k, ++shift) {
    for (int i = 0, j = shift; i < kFilterSize; ++i, j -= kStride) {
      out[k] += state[kMemorySize + j] * filter[i];
    }
  }

  for (; k < kFilterSize * kStride; ++k, ++shift) {
    RTC_DCHECK_LE(0, shift);
    const int loop_limit = std::min(kFilterSize, 1 + (shift >> kStrideLog2));
    for (int i = 0, j = shift; i < loop_limit; ++i, j -= kStride) {
      out[k] += in[j] * filter[i];
    }
    for (int i = loop_limit, j = shift - loop_limit * kStride; i < kFilterSize;
         ++i, j -= kStride) {
      out[k] += state[kMemorySize + j] * filter[i];
    }
  }

  for (; k < kSplitBandSize; ++k, ++shift) {
    for (int i = 0, j = shift; i < kFilterSize; ++i, j -= kStride) {
      out[k] += in[j] * filter[i];
    }
  }

  // Update current state.
  std::copy(in.begin() + kSplitBandSize - kMemorySize, in.end(), state.begin());
}

}  // namespace

// Because the low-pass filter prototype has half bandwidth it is possible to
// use a DCT to shift it in both directions at the same time, to the center
// frequencies [1 / 12, 3 / 12, 5 / 12].
ThreeBandFilterBank::ThreeBandFilterBank(size_t length) {
  RTC_DCHECK_EQ(kNumFilters, state_analysis_.size());
  RTC_DCHECK_EQ(kNumFilters, state_synthesis_.size());
  for (int k = 0; k < kNumFilters; ++k) {
    RTC_DCHECK_EQ(kMemorySize, state_analysis_[k].size());
    RTC_DCHECK_EQ(kMemorySize, state_synthesis_[k].size());

    state_analysis_[k].fill(0.f);
    state_synthesis_[k].fill(0.f);
  }
}

ThreeBandFilterBank::~ThreeBandFilterBank() = default;

// The analysis can be separated in these steps:
//   1. Serial to parallel downsampling by a factor of |3|.
//   2. Filtering of |4| different delayed signals with polyphase
//      decomposition of the low-pass prototype filter and upsampled by a factor
//      of |4|.
//   3. Modulating with cosines and accumulating to get the desired band.
void ThreeBandFilterBank::Analysis(const float* in,
                                   size_t length,
                                   float* const* out) {
  RTC_DCHECK_EQ(kFullBandSize, length);
  // Initialize the output to zero.
  for (int band = 0; band < kNumBands; ++band) {
    memset(out[band], 0, kSplitBandSize * sizeof(float));
  }

  for (int downsampling_index = 0; downsampling_index < kSubSampling;
       ++downsampling_index) {
    // Prepare filter input.
    std::array<float, kSplitBandSize> in_subsampled;
    for (int k = 0; k < kSplitBandSize; ++k) {
      in_subsampled[k] =
          in[(kSubSampling - 1) - downsampling_index + kSubSampling * k];
    }

    for (int buffer_shift = 0; buffer_shift < kStride; ++buffer_shift) {
      // Choose filter, skip zero filters.
      const int index = downsampling_index + buffer_shift * kSubSampling;
      if (index == kZeroFilterIndex1 || index == kZeroFilterIndex2) {
        continue;
      }
      const int filter_index =
          index < kZeroFilterIndex1
              ? index
              : (index < kZeroFilterIndex2 ? index - 1 : index - 2);

      rtc::ArrayView<const float, kFilterSize> filter(
          kFilterCoeffs[filter_index]);
      rtc::ArrayView<const float, kDctSize> dct_modulation(
          kDctModulation[filter_index]);
      rtc::ArrayView<float, kMemorySize> state(state_analysis_[filter_index]);

      // Filter.
      std::array<float, kSplitBandSize> out_subsampled;
      FilterCore(-buffer_shift, filter, in_subsampled, out_subsampled, state);

      // Band and modulate the output.
      for (int band = 0; band < kNumBands; ++band) {
        for (int n = 0; n < kSplitBandSize; ++n) {
          out[band][n] += dct_modulation[band] * out_subsampled[n];
        }
      }
    }
  }
}

// The synthesis can be separated in these steps:
//   1. Modulating with cosines.
//   2. Filtering each one with a polyphase decomposition of the low-pass
//      prototype filter upsampled by a factor of |4| and accumulating
//      |4| signals with different delays.
//   3. Parallel to serial upsampling by a factor of |3|.
void ThreeBandFilterBank::Synthesis(const float* const* in,
                                    size_t split_length,
                                    float* out) {
  RTC_DCHECK_EQ(kSplitBandSize, split_length);

  memset(out, 0, kFullBandSize * sizeof(float));
  for (int upsampling_index = 0; upsampling_index < kSubSampling;
       ++upsampling_index) {
    for (int buffer_shift = 0; buffer_shift < kStride; ++buffer_shift) {
      // Choose filter, skip zero filters.
      const int index = upsampling_index + buffer_shift * kSubSampling;
      if (index == kZeroFilterIndex1 || index == kZeroFilterIndex2) {
        continue;
      }
      const int filter_index =
          index < kZeroFilterIndex1
              ? index
              : (index < kZeroFilterIndex2 ? index - 1 : index - 2);

      rtc::ArrayView<const float, kFilterSize> filter(
          kFilterCoeffs[filter_index]);
      rtc::ArrayView<const float, kDctSize> dct_modulation(
          kDctModulation[filter_index]);
      rtc::ArrayView<float, kMemorySize> state(state_synthesis_[filter_index]);

      // Prepare filter input by modulating the banded input.
      std::array<float, kSplitBandSize> in_subsampled;
      std::fill(in_subsampled.begin(), in_subsampled.end(), 0.f);
      for (int band = 0; band < kNumBands; ++band) {
        for (int n = 0; n < kSplitBandSize; ++n) {
          in_subsampled[n] += dct_modulation[band] * in[band][n];
        }
      }

      // Filter.
      std::array<float, kSplitBandSize> out_subsampled;
      FilterCore(-buffer_shift, filter, in_subsampled, out_subsampled, state);

      // Upsample.
      constexpr float kUpsamplingScaling = kSubSampling;
      for (int k = 0; k < kSplitBandSize; ++k) {
        out[upsampling_index + kSubSampling * k] +=
            kUpsamplingScaling * out_subsampled[k];
      }
    }
  }
}

}  // namespace webrtc
