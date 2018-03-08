/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_FILTER_ANALYZER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_FILTER_ANALYZER_H_

#include <vector>

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/optional.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class FilterAnalyzer {
 public:
  explicit FilterAnalyzer(const EchoCanceller3Config& config);
  ~FilterAnalyzer();
  void Reset();
  void Update(rtc::ArrayView<const float> filter_time_domain,
              const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                  filter_frequency_response,
              bool converged_filter,
              bool clock_drift);
  int DelayBlocks() const { return delay_blocks_; }
  const rtc::Optional<float>& Gain() const { return gain_; }
  bool GoodEstimate() const { return good_estimate_; }

 private:
  const bool bounded_erl_;
  bool converged_filter_seen_ = false;
  size_t blocks_since_converged_filter_seen_ = 0;
  int delay_blocks_ = 0;
  rtc::Optional<float> gain_;
  size_t blocks_since_reset_ = 0;
  bool good_estimate_ = false;
  RTC_DISALLOW_COPY_AND_ASSIGN(FilterAnalyzer);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_FILTER_ANALYZER_H_
