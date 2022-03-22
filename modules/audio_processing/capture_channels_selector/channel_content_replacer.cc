/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_channels_selector/channel_content_replacer.h"

#include "api/array_view.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

// Default value used to initialize the values related to the number of samples
// per channel.
constexpr int kDefaultNumSamplesPerChannel = 480;

}  // namespace

ChannelContentReplacer::ChannelContentReplacer(int channel_to_replace)
    : channel_to_replace_(channel_to_replace),
      previous_channel_used_as_replacement_(channel_to_replace_),
      one_by_num_samples_per_channel_(kDefaultNumSamplesPerChannel) {}

void ChannelContentReplacer::ReplaceChannelContent(int channel_to_replace_from,
                                                   AudioBuffer& audio_buffer) {
  RTC_DCHECK_GT(audio_buffer.num_channels(), channel_to_replace_);
  RTC_DCHECK_GT(audio_buffer.num_channels(), channel_to_replace_from);
  RTC_DCHECK_GT(audio_buffer.num_channels(),
                previous_channel_used_as_replacement_);
  RTC_DCHECK_EQ(one_by_num_samples_per_channel_,
                1.0f / audio_buffer.num_frames());

  const bool replacement_needed =
      channel_to_replace_from != channel_to_replace_ ||
      previous_channel_used_as_replacement_ != channel_to_replace_;

  if (!replacement_needed) {
    return;
  }

  const bool use_cross_fading =
      previous_channel_used_as_replacement_ != channel_to_replace_from;

  if (use_cross_fading) {
    ReplacementByCrossFade(channel_to_replace_from, audio_buffer);
  } else {
    ReplacementByCopy(channel_to_replace_from, audio_buffer);
  }

  previous_channel_used_as_replacement_ = channel_to_replace_from;
}

void ChannelContentReplacer::ReplacementByCrossFade(
    int channel_to_replace_from,
    AudioBuffer& audio_buffer) const {
  rtc::ArrayView<const float> source_channel_fade_from_data(
      &audio_buffer.channels_const()[previous_channel_used_as_replacement_][0],
      audio_buffer.num_frames());
  rtc::ArrayView<const float> source_channel_fade_into_data(
      &audio_buffer.channels_const()[channel_to_replace_from][0],
      audio_buffer.num_frames());
  rtc::ArrayView<float> destination_channel_data(
      &audio_buffer.channels()[channel_to_replace_][0],
      audio_buffer.num_frames());

  for (int k = 0; k < static_cast<int>(audio_buffer.num_frames()); ++k) {
    float scaling = k * one_by_num_samples_per_channel_;
    destination_channel_data[k] =
        scaling * source_channel_fade_into_data[k] +
        (1.0f - scaling) * source_channel_fade_from_data[k];
  }
}

void ChannelContentReplacer::ReplacementByCopy(
    int channel_to_replace_from,
    AudioBuffer& audio_buffer) const {
  rtc::ArrayView<const float> source_channel_data(
      &audio_buffer.channels_const()[channel_to_replace_from][0],
      audio_buffer.num_frames());
  rtc::ArrayView<float> destination_channel_data(
      &audio_buffer.channels()[channel_to_replace_][0],
      audio_buffer.num_frames());

  std::copy(source_channel_data.begin(), source_channel_data.end(),
            destination_channel_data.begin());
}

void ChannelContentReplacer::Reset() {
  previous_channel_used_as_replacement_ = channel_to_replace_;
}

void ChannelContentReplacer::SetAudioProperties(
    const AudioBuffer& audio_buffer) {
  one_by_num_samples_per_channel_ = 1.0f / audio_buffer.num_frames();
}

}  // namespace webrtc
