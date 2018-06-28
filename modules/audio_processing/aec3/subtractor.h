/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_SUBTRACTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_SUBTRACTOR_H_

#include <algorithm>
#include <array>
#include <vector>

#include <cmath>

#include "modules/audio_processing/aec3/adaptive_fir_filter.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/echo_path_variability.h"
#include "modules/audio_processing/aec3/main_filter_update_gain.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/shadow_filter_update_gain.h"
#include "modules/audio_processing/aec3/subtractor_output.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/audio_processing/utility/ooura_fft.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// Proves linear echo cancellation functionality
class Subtractor {
 public:
  Subtractor(const EchoCanceller3Config& config,
             ApmDataDumper* data_dumper,
             Aec3Optimization optimization);
  ~Subtractor();

  // Performs the echo subtraction.
  void Process(const RenderBuffer& render_buffer,
               const rtc::ArrayView<const float> capture,
               const RenderSignalAnalyzer& render_signal_analyzer,
               const AecState& aec_state,
               SubtractorOutput* output);

  void HandleEchoPathChange(const EchoPathVariability& echo_path_variability);

  // Exits the initial state.
  void ExitInitialState();

  // Returns the block-wise frequency response for the main adaptive filter.
  const std::vector<std::array<float, kFftLengthBy2Plus1>>&
  FilterFrequencyResponse() const {
    return main_filter_once_converged_ || (!shadow_filter_converged_)
               ? main_filter_.FilterFrequencyResponse()
               : shadow_filter_.FilterFrequencyResponse();
  }

  // Returns the estimate of the impulse response for the main adaptive filter.
  const std::vector<float>& FilterImpulseResponse() const {
    return main_filter_once_converged_ || (!shadow_filter_converged_)
               ? main_filter_.FilterImpulseResponse()
               : shadow_filter_.FilterImpulseResponse();
  }

  bool ConvergedFilter() const {
    return main_filter_converged_ || shadow_filter_converged_;
  }

  bool DivergedFilter() const { return main_filter_diverged_; }

  void DumpFilters() {
    main_filter_.DumpFilter("aec3_subtractor_H_main", "aec3_subtractor_h_main");
    shadow_filter_.DumpFilter("aec3_subtractor_H_shadow",
                              "aec3_subtractor_h_shadow");
  }

 private:
  class OverEchoEstimationDetector {
   public:
    OverEchoEstimationDetector() = default;
    ~OverEchoEstimationDetector() = default;
    void Update(float e2, float y2);
    //Over 2 for doing a more conservative adjustment.
    float GetFactor() const { return sqrt(over_estimation_factor_)/2; }
    void Reset();

   private:
    const int n_blocks_ = 4;
    int n_blocks_acum_ = 0;
    float e2_acum_ = 0.f;
    float y2_acum_ = 0.f;
    float over_estimation_factor_ = 0.f;
  };

  const Aec3Fft fft_;
  ApmDataDumper* data_dumper_;
  const Aec3Optimization optimization_;
  const EchoCanceller3Config config_;
  AdaptiveFirFilter main_filter_;
  AdaptiveFirFilter shadow_filter_;
  MainFilterUpdateGain G_main_;
  ShadowFilterUpdateGain G_shadow_;
  bool main_filter_converged_ = false;
  bool main_filter_once_converged_ = false;
  bool shadow_filter_converged_ = false;
  bool main_filter_diverged_ = false;
  OverEchoEstimationDetector over_estimation_detector_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(Subtractor);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_SUBTRACTOR_H_
