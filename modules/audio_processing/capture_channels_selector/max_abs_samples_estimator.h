/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_MAX_ABS_SAMPLES_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_MAX_ABS_SAMPLES_ESTIMATOR_H_

#include <stddef.h>

#include <vector>

#include "modules/audio_processing/audio_buffer.h"

namespace webrtc {

// Estimates maximum absolute values of the samples for the channels in an
// `AudioBuffer`.
class MaxAbsSamplesEstimator {
 public:
  MaxAbsSamplesEstimator();
  MaxAbsSamplesEstimator(const MaxAbsSamplesEstimator&) = delete;
  MaxAbsSamplesEstimator& operator=(const MaxAbsSamplesEstimator&) = delete;

  // Updates the estimates of the maximum absolute values of the samples in each
  // channel based on the content in `audio_buffer`. Any DC-levels in
  // `dc_levels` are subtracted before the estimation.
  void Update(const AudioBuffer& audio_buffer,
              rtc::ArrayView<const float> dc_levels);

  // Updates the estimates of the maximum absolute values of the samples in each
  // channel based on the content in `audio_buffer`. This is to be used instead
  // of the above method when no information about DC levels is available. This
  // method should not be used after `AdjustForDcLevels` has been called,
  // without a call to `Reset`inbetween.
  void Update(const AudioBuffer& audio_buffer);

  // Compensates the estimates by adjusting for any dc-levels in `dc_levels`.
  // This is intended to be called before the first time the `Update` method
  // that includes the `dc_levels` is used.
  void AdjustForDcLevels(rtc::ArrayView<const float> dc_levels);

  // Returns the estimated maximum absolute values of the samples.
  const std::vector<float>& GetMaxAbsSampleInChannels() const {
    return max_abs_samples_in_channels_;
  }

  // Resets the estimates.
  void Reset();
  // Specifies the audio properties to use to match that of 'audio_buffer`.
  void SetAudioProperties(const AudioBuffer& audio_buffer);

 private:
  bool adjusted_for_dc_levels_ = false;
  std::vector<float> max_abs_samples_in_channels_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_MAX_ABS_SAMPLES_ESTIMATOR_H_
