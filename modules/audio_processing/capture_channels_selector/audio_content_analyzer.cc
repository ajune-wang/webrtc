/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_channels_selector/audio_content_analyzer.h"

#include <math.h>

#include <algorithm>
#include <limits>

#include "modules/audio_processing/audio_buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {
  // Empirical threshold for the number of frames that has to be analyzed for a
  // sufficiently reliable energy estimate to be obtained.
  constexpr int kNumFramesAnalyzedForReliableEstimates = 100;

}  // namespace

bool AudioContentAnalyzer::Analyze(const AudioBuffer& audio_buffer) {
  RTC_DCHECK(!audio_buffer.IsBandSplit());
  ++num_frames_analyzed_;

  // Exclude the first frame from the analysis to avoid reacting on any
  // uninitialized buffer content.
  constexpr int kNumFramesToExcludeAtStartup = 1;
  if (num_frames_analyzed_ <= kNumFramesToExcludeAtStartup) {
    return false;
  }

  const bool reliable_dc_estimate = dc_levels_estimator_.Update(audio_buffer);

  if (!reliable_dc_estimate) {
    max_abs_samples_estimator_.Update(audio_buffer);
    return false;
  }

  rtc::ArrayView<const float> dc_levels = dc_levels_estimator_.GetLevels();

  if (!previous_analysis_was_reliable_) {
    max_abs_samples_estimator_.AdjustForDcLevels(dc_levels);
    previous_analysis_was_reliable_ = true;
  }

  average_energy_estimator_.Update(audio_buffer, dc_levels);
  max_abs_samples_estimator_.Update(audio_buffer, dc_levels);
  return true;
}

void AudioContentAnalyzer::Reset() {
  previous_analysis_was_reliable_ = false;
  num_frames_analyzed_ = 0;
  num_frames_analyzed_using_dc_estimates_ = 0;
  dc_levels_estimator_.Reset();
  average_energy_estimator_.Reset();
  max_abs_samples_estimator_.Reset();
}

void AudioContentAnalyzer::SetAudioProperties(const AudioBuffer& audio_buffer) {
  dc_levels_estimator_.SetAudioProperties(audio_buffer);
  average_energy_estimator_.SetAudioProperties(audio_buffer);
  max_abs_samples_estimator_.SetAudioProperties(audio_buffer);
}

bool AudioContentAnalyzer::ReliableEnergyEstimatesAvailable() const {
  return num_frames_analyzed_using_dc_estimates_ >= kNumFramesAnalyzedForReliableEstimates;
}

}  // namespace webrtc
