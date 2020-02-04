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

// The Matlab code to generate these |kLowpassCoeffs| is:
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
const float kLowpassCoeffs[10][4] = {
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

const float kDctModulation[10][3] = {{2.f, 2.f, 2.f},
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
                rtc::ArrayView<const float, 4> filter,
                rtc::ArrayView<const float, 160> in,
                rtc::ArrayView<float, 160> out,
                rtc::ArrayView<float, 15> state) {
  memset(out.data(), 0, 160 * sizeof(float));

  int k = 0;
  int shift = shift_start;
  for (; k < shift_start; ++k, ++shift) {
    for (int i = 0, j = shift; i < 4; ++i, j -= 4) {
      out[k] += state[15 + j] * filter[i];
    }
  }

  for (; k < 16; ++k, ++shift) {
    int loop_limit = std::min(4, 1 + (shift >> 2));
    for (int i = 0, j = shift; i < loop_limit; ++i, j -= 4) {
      out[k] += in[j] * filter[i];
    }
    for (int i = loop_limit, j = shift - loop_limit * 4; i < 4; ++i, j -= 4) {
      out[k] += state[15 + j] * filter[i];
    }
  }

  for (; k < 160; ++k, ++shift) {
    for (int i = 0, j = shift; i < 4; ++i, j -= 4) {
      out[k] += in[j] * filter[i];
    }
  }

  // Update current state.
  std::memcpy(&state[0], &in[145], 15 * sizeof(float));
}

}  // namespace

// Because the low-pass filter prototype has half bandwidth it is possible to
// use a DCT to shift it in both directions at the same time, to the center
// frequencies [1 / 12, 3 / 12, 5 / 12].
ThreeBandFilterBank::ThreeBandFilterBank(size_t length) {
  for (int index = 0; index < 10; ++index) {
    state_analysis_[index].fill(0.f);
    state_synthesis_[index].fill(0.f);
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
  RTC_DCHECK_EQ(480, length);
  // Initialize the output to zero.
  for (int i = 0; i < 3; ++i) {
    memset(out[i], 0, 160 * sizeof(float));
  }

  for (int downsampling_index = 0; downsampling_index < 3;
       ++downsampling_index) {
    // Prepare filter input.
    std::array<float, 160> in_subsampled;
    for (int k = 0; k < 160; ++k) {
      in_subsampled[k] = in[2 - downsampling_index + 3 * k];
    }

    for (int buffer_shift = 0; buffer_shift < 4; ++buffer_shift) {
      // Choose filter, skip zero filters.
      const int index = downsampling_index + buffer_shift * 3;
      if (index == 3 || index == 9) {
        continue;
      }
      const int filter_index =
          index < 3 ? index : (index < 9 ? index - 1 : index - 2);

      rtc::ArrayView<const float, 4> filter(&kLowpassCoeffs[filter_index][0],
                                            4);
      rtc::ArrayView<const float, 3> dct_modulation(
          &kDctModulation[filter_index][0], 3);
      rtc::ArrayView<float, 15> state(&state_analysis_[filter_index][0], 15);

      // Filter.
      std::array<float, 160> out_subsampled;
      FilterCore(-buffer_shift, filter, in_subsampled, out_subsampled, state);

      // Band and modulate the output.
      for (int band = 0; band < 3; ++band) {
        for (int n = 0; n < 160; ++n) {
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
  RTC_DCHECK_EQ(160, split_length);

  memset(out, 0, 480 * sizeof(float));
  for (int upsampling_index = 0; upsampling_index < 3; ++upsampling_index) {
    for (int buffer_shift = 0; buffer_shift < 4; ++buffer_shift) {
      // Choose filter, skip zero filters.
      const int index = upsampling_index + buffer_shift * 3;
      if (index == 3 || index == 9) {
        continue;
      }
      const int filter_index =
          index < 3 ? index : (index < 9 ? index - 1 : index - 2);

      rtc::ArrayView<const float, 4> filter(&kLowpassCoeffs[filter_index][0],
                                            4);
      rtc::ArrayView<const float, 3> dct_modulation(
          &kDctModulation[filter_index][0], 3);
      rtc::ArrayView<float, 15> state(&state_synthesis_[filter_index][0], 15);

      // Prepare filter input by modulating the banded input.
      std::array<float, 160> in_subsampled;
      memset(in_subsampled.data(), 0, 160 * sizeof(float));
      for (int band = 0; band < 3; ++band) {
        for (int n = 0; n < 160; ++n) {
          in_subsampled[n] += dct_modulation[band] * in[band][n];
        }
      }

      // Filter.
      std::array<float, 160> out_subsampled;
      FilterCore(-buffer_shift, filter, in_subsampled, out_subsampled, state);

      // Upsample.
      for (int k = 0; k < 160; ++k) {
        out[upsampling_index + 3 * k] += 3.f * out_subsampled[k];
      }
    }
  }
}

}  // namespace webrtc
