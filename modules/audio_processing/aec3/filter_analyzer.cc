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

FilterAnalyzer::FilterAnalyzer() {
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
}

void FilterAnalyzer::Update(rtc::ArrayView<const float> filter_time_domain,
                            bool converged_filter) {
  size_t peak_index = FindPeakIndex(filter_time_domain);

  float filter_checksum = 0;
  for (auto h_k : filter_time_domain) {
    filter_checksum += h_k;
  }
  delay_blocks_ = peak_index / kBlockSize;

  DetectConsistentFilter(delay_blocks_, filter_checksum);

  if (!Consistent()) {
    delay_blocks_ = 1;
  }
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

  consistent_estimate_ = consistent_estimate_counter_ > 1 * 250;
}

}  // namespace webrtc
