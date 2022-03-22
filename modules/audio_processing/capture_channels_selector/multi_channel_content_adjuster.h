/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_MULTI_CHANNEL_CONTENT_ADJUSTER_H_
#define MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_MULTI_CHANNEL_CONTENT_ADJUSTER_H_

#include <stddef.h>

#include <vector>

#include "absl/types/optional.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/capture_channels_selector/audio_content_analyzer.h"
#include "modules/audio_processing/capture_channels_selector/channel_content_replacer.h"

namespace webrtc {

// Provides functionality selectively replaceing mic channels in `audio_buffer` containing audio content unsuitable for a multichannel signal.
class MultiChannelContentAdjuster {
 public:
  MultiChannelContentAdjuster();
  MultiChannelContentAdjuster(const MultiChannelContentAdjuster&) = delete;
  MultiChannelContentAdjuster& operator=(const MultiChannelContentAdjuster&) = delete;

  // Selectively replaces mic channels in `audio_buffer` containing audio content unsuitable for a multichannel signal. This is done based on the findings made in the `Analyze` call. One example of action taken is that for a stereo signal with silent right channel, the content in the right channel will be replaced by the left channel. The content of `audio` must not be band-split (this is only enforced via a DCHECK and has the effect that none of the operations performed has any effect on the `audio_buffer`).
  void HandleUnsuitableMicChannels(AudioBuffer& audio_buffer);

  // Resets the channel quality assessment.
  void Reset();

 private:
  // Detects changes in the configuration of `audio_buffer` and applies the appropriate state adjustments.
  void ReactToAudioFormatChanges(const AudioBuffer& audio_buffer);

  AudioContentAnalyzer audio_content_analyzer_;
  ChannelContentReplacer channel_0_content_replacer_;
  ChannelContentReplacer channel_1_content_replacer_;
  int num_channels_;
  int num_samples_per_channel_;
  float one_by_num_samples_per_channel_;
  bool channel_0_replaced_last_frame_ = false;
  bool channel_1_replaced_last_frame_ = false;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_MULTI_CHANNEL_CONTENT_ADJUSTER_H_
