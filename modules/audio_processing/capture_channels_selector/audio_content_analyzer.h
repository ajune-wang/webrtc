/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_AUDIO_CONTENT_ANALYZER_H_
#define MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_AUDIO_CONTENT_ANALYZER_H_

#include <stddef.h>

#include <vector>

#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/capture_channels_selector/average_energy_estimator.h"
#include "modules/audio_processing/capture_channels_selector/dc_levels_estimator.h"
#include "modules/audio_processing/capture_channels_selector/max_abs_samples_estimator.h"

namespace webrtc {

// Analyzes the audio content in audio buffers to produce values for the maximum
// absolute values of samples and average energies that are observed in the
// separate channels of the audio buffer.
class AudioContentAnalyzer {
 public:
  AudioContentAnalyzer() = default;
  AudioContentAnalyzer(const AudioContentAnalyzer&) = delete;
  AudioContentAnalyzer& operator=(const AudioContentAnalyzer&) = delete;

  // Analyzes the audio in `audio_buffer` to determine the properties and
  // quality of the channel audio content. The content of `audio` must not be
  // band-split (this is only enforced via a DCHECK but if the channel is
  // band-split, it will have the effect that the wrong audio is analyzed). A bool indicating whether the estimates are reliable is returned.
  bool Analyze(const AudioBuffer& audio_buffer);

  // Resets the analysis.
  void Reset();

  // Specifies the audio properties to use to match that of 'audio_buffer`.
  void SetAudioProperties(const AudioBuffer& audio_buffer);

  // Returns the identified maximum absolute values of the samples in each
  // channel.
  const std::vector<float>& GetMaxAbsSampleInChannels() const {
    return max_abs_samples_estimator_.GetMaxAbsSampleInChannels();
  }

  // Returns the estimated average energies in each channel.
  const std::vector<float>& GetChannelEnergies() const {
    return average_energy_estimator_.GetChannelEnergies();
  }

  // Returns whether reliable average energy estimates are available.
  bool ReliableEnergyEstimatesAvailable() const;

 private:
  int num_frames_analyzed_;
  int num_frames_analyzed_using_dc_estimates_;
  bool previous_analysis_was_reliable_;
  DcLevelsEstimator dc_levels_estimator_;
  AverageEnergyEstimator average_energy_estimator_;
  MaxAbsSamplesEstimator max_abs_samples_estimator_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_AUDIO_CONTENT_ANALYZER_H_
