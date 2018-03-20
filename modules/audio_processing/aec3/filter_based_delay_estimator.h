/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_FILTER_BASED_DELAY_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_FILTER_BASED_DELAY_ESTIMATOR_H_

#include <algorithm>
#include <array>
#include <vector>

#include "modules/audio_processing/aec3/adaptive_fir_filter.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/filter_analyzer.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/shadow_filter_update_gain.h"
#include "modules/audio_processing/aec3/subtractor_output.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/audio_processing/utility/ooura_fft.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class FilterBasedDelayEstimator {
 public:
  explicit FilterBasedDelayEstimator(const EchoCanceller3Config& config,
                                     ApmDataDumper* data_dumper,
                                     Aec3Optimization optimization);
  ~FilterBasedDelayEstimator();

  void Reset();
  rtc::Optional<int> DelayBlocks() { return delay_blocks_; }

  void Update(const RenderBuffer& render_buffer,
              const rtc::ArrayView<const float> capture,
              const RenderSignalAnalyzer& render_signal_analyzer,
              const AecState& aec_state);

  void DumpFilter() {
    filter_.DumpFilter("aec3_refined_delay_estimator_H",
                       "aec3_filter_based_delay_estimator.h");
  }

 private:
  const Aec3Fft fft_;
  const Aec3Optimization optimization_;
  const EchoCanceller3Config config_;
  rtc::Optional<int> delay_blocks_;
  AdaptiveFirFilter filter_;
  ShadowFilterUpdateGain G_filter_;
  FilterAnalyzer filter_analyzer_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FilterBasedDelayEstimator);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_FILTER_BASED_DELAY_ESTIMATOR_H_
