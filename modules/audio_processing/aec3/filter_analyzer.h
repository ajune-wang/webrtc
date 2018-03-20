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

// Class for analyzing the   properties of an adaptive filter.
class FilterAnalyzer {
 public:
  FilterAnalyzer();
  ~FilterAnalyzer();

  // Resets the analysis.
  void Reset();

  // Updates the estimates with new input data.
  void Update(rtc::ArrayView<const float> filter_time_domain,
              bool converged_filter);

  // Returns the delay of the filter in terms of blocks.
  int DelayBlocks() const { return delay_blocks_; }

  // Returns whether the filter is consistent in the sense that it does not
  // change much over time.
  bool Consistent() const { return consistent_estimate_; }

 private:
  void DetectConsistentFilter(int delay_blocks, float filter_checksum);

  bool converged_filter_seen_ = false;
  int delay_blocks_ = 0;
  size_t blocks_since_reset_ = 0;
  bool consistent_estimate_ = false;
  size_t consistent_estimate_counter_ = 0;
  int consistent_delay_reference_ = -10;
  float consistent_checksum_reference_ = 0;

  RTC_DISALLOW_COPY_AND_ASSIGN(FilterAnalyzer);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_FILTER_ANALYZER_H_
