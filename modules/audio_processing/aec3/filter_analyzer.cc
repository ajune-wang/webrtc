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

#include "rtc_base/checks.h"

#include <math.h>

#include <algorithm>
#include <array>
#include <numeric>

namespace webrtc {
namespace {

// Computes delay of the adaptive filter.
int EstimateFilterDelay(
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        adaptive_filter_frequency_response) {
  const auto& H2 = adaptive_filter_frequency_response;
  constexpr size_t kUpperBin = kFftLengthBy2 - 5;
  RTC_DCHECK_GE(kMaxAdaptiveFilterLength, H2.size());
  std::array<int, kMaxAdaptiveFilterLength> delays;
  delays.fill(0);
  for (size_t k = 1; k < kUpperBin; ++k) {
    // Find the maximum of H2[j].
    size_t peak = 0;
    for (size_t j = 0; j < H2.size(); ++j) {
      if (H2[j][k] > H2[peak][k]) {
        peak = j;
      }
    }
    ++delays[peak];
  }

  return std::distance(delays.begin(),
                       std::max_element(delays.begin(), delays.end()));
}

}  // namespace

FilterAnalyzer::FilterAnalyzer(const EchoCanceller3Config& config)
    : bounded_erl_(config.ep_strength.bounded_erl) {
  Reset();
}

FilterAnalyzer::~FilterAnalyzer() = default;

void FilterAnalyzer::Reset() {
  delay_blocks_ = 0;
  good_estimate_ = false;
  blocks_since_reset_ = 0;
  converged_filter_seen_ = false;
}

void FilterAnalyzer::Update(
    rtc::ArrayView<const float> filter_time_domain,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        filter_frequency_response,
    bool converged_filter,
    bool clock_drift) {
  blocks_since_converged_filter_seen_ =
      converged_filter ? 0 : blocks_since_converged_filter_seen_ + 1;
  converged_filter_seen_ = converged_filter_seen_ || converged_filter;
  if (blocks_since_converged_filter_seen_ > 10 * kNumBlocksPerSecond) {
    converged_filter_seen_ = false;
  }

  size_t max_index = 0;
  float max_h2 = filter_time_domain[0] * filter_time_domain[0];
  for (size_t k = 1; k < filter_time_domain.size(); ++k) {
    float tmp = filter_time_domain[k] * filter_time_domain[k];
    if (tmp > max_h2) {
      max_index = k;
      max_h2 = tmp;
    }
  }

  constexpr size_t kN1Size = 20;
  constexpr size_t kN2Size = 64;

  const auto sum_of_squares = [](float a, float b) { return a + b * b; };
  size_t n1_size = max_index > 1 ? std::min(kN1Size, max_index - 1) : 0;
  RTC_DCHECK_GE(filter_time_domain.size(), n1_size);
  float n1 = std::accumulate(filter_time_domain.begin(),
                             filter_time_domain.begin() + n1_size, 0.f,
                             sum_of_squares);
  size_t n2_size = std::min(kN2Size, filter_time_domain.size() - max_index);
  RTC_DCHECK_GE(filter_time_domain.size(), n2_size);
  float n2 = std::accumulate(filter_time_domain.end() - n2_size,
                             filter_time_domain.end(), 0.f, sum_of_squares);

  constexpr float kOneByN1Size = 1.f / kN1Size;
  constexpr float kOneByN2Size = 1.f / kN2Size;
  n1 = n1_size == kN1Size ? n1 * kOneByN1Size : n1 / n1_size;
  n2 = n2_size == kN2Size ? n2 * kOneByN2Size : n2 / n2_size;

  good_estimate_ = converged_filter_seen_ && max_h2 > 10 * std::max(n1, n2);

  if (++blocks_since_reset_ > 5 * kNumBlocksPerSecond && good_estimate_) {
    gain_ = fabsf(filter_time_domain[max_index]);
  } else {
    if (gain_) {
      gain_ = std::max(*gain_, fabsf(filter_time_domain[max_index]));
    }
  }

  if (bounded_erl_ && gain_) {
    *gain_ = std::max(*gain_, 0.01f);
  }

  if (good_estimate_) {
    delay_blocks_ = EstimateFilterDelay(filter_frequency_response);
  } else {
    delay_blocks_ = 0;
  }
}

}  // namespace webrtc
