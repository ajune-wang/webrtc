/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ERLE_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ERLE_ESTIMATOR_H_

#include <array>
#include <memory>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/erle_per_frequency_estimator.h"
#include "modules/audio_processing/aec3/erle_time_estimator.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// Estimates the echo return loss enhancement. An estimate is done per frequency
// band and another one using all the bands.
class ErleEstimator {
 public:
  ErleEstimator(float min_erle, float max_erle_lf, float max_erle_hf);
  ~ErleEstimator();

  // Reset the ERLE estimator.
  void Reset();

  // Updates the ERLE estimates.
  void Update(rtc::ArrayView<const float> render_spectrum,
              rtc::ArrayView<const float> capture_spectrum,
              rtc::ArrayView<const float> subtractor_spectrum,
              bool converged_filter,
              bool onset_detection);

  // Returns the most recent ERLE per frequency band estimate.
  const std::array<float, kFftLengthBy2Plus1>& Erle() const {
    return erle_freq_estimator_.Erle();
  }
  // Returns the ERLE per frequency band that is estimated during onsets. Use
  // for logging/testing.
  const std::array<float, kFftLengthBy2Plus1>& ErleOnsets() const {
    return erle_freq_estimator_.ErleOnsets();
  }

  // Returns the ERLE estimated when all the frequency bands are used for the
  // estimation.
  float ErleTimeDomainLog2() const {
    return erle_time_estimator_.ErleTimeDomainLog2();
  }

  // Returns an estimation of the current linear filter quality based on the
  // current and past ERLE estimations.
  absl::optional<float> GetInstLinearQualityEstimate() const {
    return erle_time_estimator_.GetInstLinearQualityEstimate();
  }

  void Dump(const std::unique_ptr<ApmDataDumper>& data_dumper) const;

 private:
  ErleTimeEstimator erle_time_estimator_;
  ErlePerFrequencyEstimator erle_freq_estimator_;
  RTC_DISALLOW_COPY_AND_ASSIGN(ErleEstimator);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ERLE_ESTIMATOR_H_
