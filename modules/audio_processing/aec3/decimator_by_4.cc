/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/decimator_by_4.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// [B,A] = butter(2,1500/16000) which are the same as [B,A] =
// butter(2,750/8000).
const CascadedBiQuadFilter::BiQuadCoefficients kLowPassFilterCoefficients4 = {
    {0.0179f, 0.0357f, 0.0179f},
    {-1.5879f, 0.6594f}};
constexpr int kNumFilters4 = 3;

const CascadedBiQuadFilter::BiQuadCoefficients kLowPassFilterCoefficients16 = {
    {0.02482791f, 0.04965581f, 0.02482791f},
    {-1.5074474f, 0.60675903f}};
constexpr int kNumFilters16 = 5;

}  // namespace

DecimatorBy4::DecimatorBy4(size_t down_sampling_factor)
    : down_sampling_factor_(down_sampling_factor),
      low_pass_filter_(
          down_sampling_factor_ == 4 ? kLowPassFilterCoefficients4
                                     : kLowPassFilterCoefficients16,
          down_sampling_factor_ == 4 ? kNumFilters4 : kNumFilters16)
    : {
  RTC_DCHECK(down_sampling_factor_ == 4 || down_sampling_factor_ == 16);
}

void DecimatorBy4::Decimate(rtc::ArrayView<const float> in,
                            rtc::ArrayView<float> out) {
  RTC_DCHECK_EQ(kBlockSize, in.size());
  RTC_DCHECK_EQ(kSubBlockSize, out.size());
  std::array<float, kBlockSize> x;

  // Limit the frequency content of the signal to avoid aliasing.
  low_pass_filter_.Process(in, x);

  // Downsample the signal.
  for (size_t j = 0, k = 0; j < out.size(); ++j, k += down_sampling_factor_) {
    RTC_DCHECK_GT(kBlockSize, k);
    out[j] = x[k];
  }
}

}  // namespace webrtc
