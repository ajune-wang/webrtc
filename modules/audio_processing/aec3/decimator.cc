/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/decimator.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// signal.ellip(6, 1, 40, 3750/8000, btype='lowpass', analog=False)
const std::vector<CascadedBiQuadFilter::BiQuadParam> kLowPassFilter2 = {
    {{-0.08873842f, 0.99605496f},
     {0.75916227f, 0.23841065f},
     0.2625069682685461f},
    {{0.62273832f, 0.78243018f},
     {0.74892112f, 0.5410152f},
     0.2625069682685461f},
    {{0.71107693f, 0.70311421f},
     {0.74895534f, 0.63924616f},
     0.2625069682685461f}};

// signal.ellip(6, 1, 40, 1800/8000, btype='lowpass', analog=False)
const std::vector<CascadedBiQuadFilter::BiQuadParam> kLowPassFilter4 = {
    {{-0.75642972f, 0.65407498f},
     {0.43532888f, 0.41488166f},
     0.40058316550825496f},
    {{-0.167777f, 0.98582497f},
     {0.19368069f, 0.85503363f},
     0.40058316550825496f},
    {{-0.00948484f, 0.99995502f},
     {0.09602716f, 0.97186158f},
     0.40058316550825496f}};

// signal.cheby1(5, 1, [1200/8000, 1800/8000], btype='bandpass', analog=False)
const std::vector<CascadedBiQuadFilter::BiQuadParam> kBandPassFilter8 = {
    {{1.f, 0.f}, {0.88297809f, 0.45085848f}, 0.07585061287925589f, true},
    {{1.f, 0.f}, {0.85178016f, 0.47599054f}, 0.07585061287925589f, true},
    {{1.f, 0.f}, {0.80953545f, 0.52763149f}, 0.07585061287925589f, true},
    {{1.f, 0.f}, {0.75203855f, 0.64045356f}, 0.07585061287925589f, true},
    {{1.f, 0.f}, {0.76954785f, 0.59024877f}, 0.07585061287925589f, true}};

// signal.butter(2, 1000/8000, btype='highpass', analog=False)
const std::vector<CascadedBiQuadFilter::BiQuadParam> kHighPassFilter = {
    {{1.f, 0.f}, {0.72712179f, 0.21296904f}, 0.7570763753338849f}};

// Pass-through filter.
const std::vector<CascadedBiQuadFilter::BiQuadParam> kNoFilter = {};

}  // namespace

Decimator::Decimator(size_t down_sampling_factor)
    : down_sampling_factor_(down_sampling_factor),
      anti_aliasing_filter_(down_sampling_factor_ == 4
                                ? kLowPassFilter4
                                : (down_sampling_factor_ == 8
                                       ? kBandPassFilter8
                                       : kLowPassFilter2)),
      noise_reduction_filter_(down_sampling_factor_ == 8 ? kNoFilter
                                                         : kHighPassFilter) {
  RTC_DCHECK(down_sampling_factor_ == 2 || down_sampling_factor_ == 4 ||
             down_sampling_factor_ == 8);
}

void Decimator::Decimate(rtc::ArrayView<const float> in,
                         rtc::ArrayView<float> out) {
  RTC_DCHECK_EQ(kBlockSize, in.size());
  RTC_DCHECK_EQ(kBlockSize / down_sampling_factor_, out.size());
  std::array<float, kBlockSize> x;

  // Limit the frequency content of the signal to avoid aliasing.
  anti_aliasing_filter_.Process(in, x);

  // Reduce the impact of near-end noise.
  noise_reduction_filter_.Process(x);

  // Downsample the signal.
  for (size_t j = 0, k = 0; j < out.size(); ++j, k += down_sampling_factor_) {
    RTC_DCHECK_GT(kBlockSize, k);
    out[j] = x[k];
  }
}

}  // namespace webrtc
