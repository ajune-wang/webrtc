/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_CHANNEL_CONTENT_REPLACER_H_
#define MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_CHANNEL_CONTENT_REPLACER_H_

#include "modules/audio_processing/audio_buffer.h"

namespace webrtc {

// Provides functionality for in a seamless manner replacing the audio content
// in a specified `AudioBuffer` channel with the audio content in another
// channel in the same buffer.
class ChannelContentReplacer {
 public:
  // C-tor where the `channel_to_replace` specifies which channel should be
  // replaced.
  explicit ChannelContentReplacer(int channel_to_replace);
  ChannelContentReplacer(const ChannelContentReplacer&) = delete;
  ChannelContentReplacer& operator=(const ChannelContentReplacer&) = delete;

  // Replaces the content of `channel_to_replace` in `audio_buffer` with the
  // contents in `channel_to_replace_from`. When the values of
  // `channel_to_replace_from` changes between call, the transition in the
  // replacement is done in a smooth manner using cross-fading.
  void ReplaceChannelContent(int channel_to_replace_from,
                             AudioBuffer& audio_buffer);

  // Resets the channel replacement functionality.
  void Reset();

  // Specifies the audio properties to use to match that of 'audio_buffer`.
  void SetAudioProperties(const AudioBuffer& audio_buffer);

 private:
  // Replaces the `audio_buffer` content in `channel_to_replace_` with the
  // content in `channel_to_replace_from` using a smooth linear cross-fading.
  void ReplacementByCrossFade(int channel_to_replace_from,
                              AudioBuffer& audio_buffer) const;

  // Replaces the `audio_buffer` content in `channel_to_replace_` with the
  // content in `channel_to_replace_from` using a plain copy..
  void ReplacementByCopy(int channel_to_replace_from,
                         AudioBuffer& audio_buffer) const;

  const int channel_to_replace_;
  int previous_channel_used_as_replacement_;
  float one_by_num_samples_per_channel_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_CHANNEL_CONTENT_REPLACER_H_
