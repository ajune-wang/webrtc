/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/filter_analyzer.h"
#include <math.h>

#include <algorithm>
#include <array>
#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// Computes delay of the adaptive filter.
void AnalyzeFilter(const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                       adaptive_filter_frequency_response,
                   int* delay,
                   float* filter_checksum) {
  RTC_DCHECK(delay);
  RTC_DCHECK(filter_checksum);

  const auto& H2 = adaptive_filter_frequency_response;
  constexpr size_t kUpperBin = kFftLengthBy2 - 5;
  RTC_DCHECK_GE(kMaxAdaptiveFilterLength, H2.size());
  std::array<int, kMaxAdaptiveFilterLength> delays;
  delays.fill(0);
  *filter_checksum = 0;
  for (size_t k = 1; k < kUpperBin; ++k) {
    // Find the maximum of H2[j].
    size_t peak = 0;
    for (size_t j = 0; j < H2.size(); ++j) {
      *filter_checksum += H2[j][k];
      if (H2[j][k] > H2[peak][k]) {
        peak = j;
      }
    }
    ++delays[peak];
  }

  *delay = std::distance(delays.begin(),
                         std::max_element(delays.begin(), delays.end()));
}

size_t FindPeakIndex(rtc::ArrayView<const float> filter_time_domain) {
  size_t peak_index = 0;
  float max_h2 = filter_time_domain[0] * filter_time_domain[0];
  for (size_t k = 1; k < filter_time_domain.size(); ++k) {
    float tmp = filter_time_domain[k] * filter_time_domain[k];
    if (tmp > max_h2) {
      peak_index = k;
      max_h2 = tmp;
    }
  }

  return peak_index;
}

}  // namespace

FilterAnalyzer::FilterAnalyzer(const EchoCanceller3Config& config)
    : bounded_erl_(config.ep_strength.bounded_erl) {
  Reset();
}

FilterAnalyzer::~FilterAnalyzer() = default;

void FilterAnalyzer::Reset() {
  delay_blocks_ = 0;
  consistent_estimate_ = false;
  blocks_since_reset_ = 0;
  converged_filter_seen_ = false;
  consistent_estimate_ = false;
  consistent_estimate_counter_ = 0;
  consistent_delay_reference_ = -10;
  accurate_filter_ = false;
}

void FilterAnalyzer::Update(
    rtc::ArrayView<const float> filter_time_domain,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        filter_frequency_response,
    bool converged_filter,
    bool clock_drift) {
  size_t peak_index = FindPeakIndex(filter_time_domain);

  UpdateFilterGain(filter_time_domain, peak_index);

  float filter_checksum;
  AnalyzeFilter(filter_frequency_response, &delay_blocks_, &filter_checksum);

  DetectConsistentFilter(delay_blocks_, filter_checksum);

  DetectAccurateFilter(filter_time_domain, delay_blocks_, converged_filter);

  if (!Consistent()) {
    delay_blocks_ = 1;
  }
}

void FilterAnalyzer::UpdateFilterGain(
    rtc::ArrayView<const float> filter_time_domain,
    size_t peak_index) {
  bool sufficient_time_to_converge =
      ++blocks_since_reset_ > 5 * kNumBlocksPerSecond;

  if (sufficient_time_to_converge && consistent_estimate_) {
    gain_ = fabsf(filter_time_domain[peak_index]);
  } else {
    if (gain_) {
      gain_ = std::max(gain_, fabsf(filter_time_domain[peak_index]));
    }
  }

  if (bounded_erl_ && gain_) {
    gain_ = std::max(gain_, 0.01f);
  }
}

void FilterAnalyzer::DetectAccurateFilter(
    rtc::ArrayView<const float> filter_time_domain,
    size_t peak_index,
    bool converged_filter) {
  blocks_since_converged_filter_seen_ =
      converged_filter ? 0 : blocks_since_converged_filter_seen_ + 1;
  converged_filter_seen_ = converged_filter_seen_ || converged_filter;
  if (blocks_since_converged_filter_seen_ > 60 * kNumBlocksPerSecond) {
    converged_filter_seen_ = false;
  }

  constexpr size_t kN1Size = 20;
  constexpr size_t kN2Size = 64;

  const auto sum_of_squares = [](float a, float b) { return a + b * b; };
  size_t n1_size = peak_index > 1 ? std::min(kN1Size, peak_index - 1) : 0;
  RTC_DCHECK_GE(filter_time_domain.size(), n1_size);
  float n1 = std::accumulate(filter_time_domain.begin(),
                             filter_time_domain.begin() + n1_size, 0.f,
                             sum_of_squares);
  size_t n2_size = std::min(kN2Size, filter_time_domain.size() - peak_index);
  RTC_DCHECK_GE(filter_time_domain.size(), n2_size);
  float n2 = std::accumulate(filter_time_domain.end() - n2_size,
                             filter_time_domain.end(), 0.f, sum_of_squares);

  constexpr float kOneByN1Size = 1.f / kN1Size;
  constexpr float kOneByN2Size = 1.f / kN2Size;
  n1 = n1_size == kN1Size ? n1 * kOneByN1Size : n1 / n1_size;
  n2 = n2_size == kN2Size ? n2 * kOneByN2Size : n2 / n2_size;

  float h2_peak =
      filter_time_domain[peak_index] * filter_time_domain[peak_index];
  accurate_filter_ = converged_filter_seen_ && h2_peak > 10 * std::max(n1, n2);
}

void FilterAnalyzer::DetectConsistentFilter(int delay_blocks,
                                            float filter_checksum) {
  if (abs(consistent_delay_reference_ - delay_blocks) > 1) {
    consistent_delay_reference_ = delay_blocks;
    consistent_estimate_counter_ = 0;
  } else {
    if (consistent_checksum_reference_ != filter_checksum) {
      consistent_checksum_reference_ = filter_checksum;
      ++consistent_estimate_counter_;
    }
  }

  consistent_estimate_ = consistent_estimate_counter_ > 100;
}

}  // namespace webrtc
