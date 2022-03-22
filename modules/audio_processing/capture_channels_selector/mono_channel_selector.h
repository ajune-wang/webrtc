/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_MONO_CHANNEL_SELECTOR_H_
#define MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_MONO_CHANNEL_SELECTOR_H_

#include <stddef.h>

#include <vector>

#include "absl/types/optional.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/capture_channels_selector/audio_content_analyzer.h"
#include "modules/audio_processing/capture_channels_selector/channel_content_replacer.h"

namespace webrtc {

// Provides functionality for downmixing the audio in `audio_buffer` into 1
// channel by choosing the channel with the best quality.
class MonoChannelSelector {
 public:
  MonoChannelSelector();
  MonoChannelSelector(const MonoChannelSelector&) = delete;
  MonoChannelSelector& operator=(const MonoChannelSelector&) = delete;

  // Downmixes the audio in `audio_buffer` into 1 channel. The downmixing is
  // done based on the findings from the call to `Analyze`. The content of
  // `audio` must not be band-split (this is only enforced via a DCHECK and has
  // the effect that only the left channel will be chosen).
  void DownMixToBestChannel(AudioBuffer& audio_buffer);

  // Resets the channel selection functionality.
  void Reset();

 private:
  // Detects changes in the configuration of `audio_buffer` and applies the
  // appropriate state adjustments.
  void ReactToAudioFormatChanges(const AudioBuffer& audio_buffer);

  ChannelContentReplacer channel_content_replacer_;
  AudioContentAnalyzer audio_content_analyzer_;
  int num_channels_;
  int num_samples_per_channel_;
  absl::optional<float> permanently_selected_channel_;
  int previously_selected_channel_;
  int num_frames_analyzed_ = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_MONO_CHANNEL_SELECTOR_H_
