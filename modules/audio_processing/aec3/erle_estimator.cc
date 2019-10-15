/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/erle_estimator.h"

#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/checks.h"

namespace webrtc {

ErleEstimator::ErleEstimator(size_t startup_phase_length_blocks_,
                             const EchoCanceller3Config& config,
                             size_t num_capture_channels)
    : use_onset_detection_(config.erle.onset_detection),
      startup_phase_length_blocks__(startup_phase_length_blocks_),
      use_signal_dependent_erle_(config.erle.num_sections > 1),
      fullband_erle_estimator_(config.erle, num_capture_channels),
      subband_erle_estimator_(config, num_capture_channels),
      signal_dependent_erle_estimator_(config, num_capture_channels) {
  Reset(true);
}

ErleEstimator::~ErleEstimator() = default;

void ErleEstimator::Reset(bool delay_change) {
  fullband_erle_estimator_.Reset();
  subband_erle_estimator_.Reset();
  signal_dependent_erle_estimator_.Reset();
  if (delay_change) {
    blocks_since_reset_ = 0;
  }
}

void ErleEstimator::Update(
    const RenderBuffer& render_buffer,
    rtc::ArrayView<const std::vector<std::array<float, kFftLengthBy2Plus1>>>
        filter_frequency_responses,
    rtc::ArrayView<const float> avg_render_spectrum_with_reverb,
    rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>> capture_spectra,
    rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>>
        subtractor_spectra,
    const std::vector<bool>& converged_filters) {
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, avg_render_spectrum_with_reverb.size());
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, capture_spectra.size());
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, subtractor_spectra.size());
  const auto& X2_reverb = avg_render_spectrum_with_reverb;
  const auto& Y2 = capture_spectra;
  const auto& E2 = subtractor_spectra;

  if (++blocks_since_reset_ < startup_phase_length_blocks__) {
    return;
  }

  subband_erle_estimator_.Update(X2_reverb, Y2[0], E2[0], converged_filters[0],
                                 use_onset_detection_);

  if (use_signal_dependent_erle_) {
    signal_dependent_erle_estimator_.Update(
        render_buffer, filter_frequency_responses[0], X2_reverb, Y2[0], E2[0],
        subband_erle_estimator_.Erle(), converged_filters[0]);
  }

  fullband_erle_estimator_.Update(X2_reverb, Y2[0], E2[0],
                                  converged_filters[0]);
}

void ErleEstimator::Dump(
    const std::unique_ptr<ApmDataDumper>& data_dumper) const {
  fullband_erle_estimator_.Dump(data_dumper);
  subband_erle_estimator_.Dump(data_dumper);
  signal_dependent_erle_estimator_.Dump(data_dumper);
}

}  // namespace webrtc
