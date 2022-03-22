
/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_DC_LEVELS_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_DC_LEVELS_ESTIMATOR_H_

#include <stddef.h>

#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/audio_buffer.h"

namespace webrtc {

// Estimates any DC levels for the channels in an `AudioBuffer`.
class DcLevelsEstimator {
public:
  DcLevelsEstimator();
  DcLevelsEstimator(const DcLevelsEstimator&) = delete;
  DcLevelsEstimator& operator=(const DcLevelsEstimator&) = delete;

  // Updates the estimates of the DC levels based on the content of `audio_buffer` and returns whether reliable estimates are available.
  bool Update(const AudioBuffer& audio_buffer);

  rtc::ArrayView<const float> GetLevels() const {
    return dc_levels_;
  }

  // Resets the estimates.
  void Reset();

  // Specifies the audio properties to use to match that of 'audio_buffer`.
  void SetAudioProperties(const AudioBuffer& audio_buffer);

private:
  int num_samples_per_channel_;
  float one_by_num_samples_per_channel_;
  int num_analyzed_frames_ = 0;
  std::vector<float> dc_levels_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_DC_LEVELS_ESTIMATOR_H_
