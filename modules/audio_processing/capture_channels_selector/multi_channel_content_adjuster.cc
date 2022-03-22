/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_channels_selector/multi_channel_content_adjuster.h"

#include <math.h>

#include <algorithm>
#include <limits>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/audio_processing/audio_buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

void CompareStereoChannelsStrengths(rtc::ArrayView<const float> average_energy,
                                    bool& channel_0_much_stronger,
                                    bool& channel_1_much_stronger,
                                    bool& channel_0_at_most_somewhat_stronger,
                                    bool& channel_1_at_most_somewhat_stronger) {
  // Check metrics indicating the balance of the channels.
  const auto is_much_stronger = [](float average_energy_0,
                                   float average_energy_1) {
    constexpr float kMuchStrongerThreshold = 10.0f;
    return average_energy_0 > kMuchStrongerThreshold * average_energy_1;
  };
  channel_0_much_stronger =
      is_much_stronger(average_energy[0], average_energy[1]);
  channel_1_much_stronger =
      is_much_stronger(average_energy[1], average_energy[0]);

  const auto is_at_most_somewhat_stronger = [](float average_energy_0,
                                               float average_energy_1) {
    constexpr float kSomewhatStrongerThreshold = 3.0f;
    return average_energy_0 < kSomewhatStrongerThreshold * average_energy_1;
  };
  channel_0_at_most_somewhat_stronger =
      is_at_most_somewhat_stronger(average_energy[0], average_energy[1]);
  channel_1_at_most_somewhat_stronger =
      is_at_most_somewhat_stronger(average_energy[1], average_energy[0]);

  RTC_DCHECK(!(channel_0_much_stronger && channel_1_much_stronger));
  RTC_DCHECK(!(channel_0_at_most_somewhat_stronger && channel_0_much_stronger));
  RTC_DCHECK(!(channel_1_at_most_somewhat_stronger && channel_1_much_stronger));
}

void MakeReplacementDecision(bool channel_0_replaced_last_frame,
                              bool channel_1_replaced_last_frame,
                              bool channel_0_much_stronger,
                              bool channel_1_much_stronger,
                              bool channel_0_at_most_somewhat_stronger,
                              bool channel_1_at_most_somewhat_stronger,
                              bool& replace_channel_0,
                              bool& replace_channel_1) {
  // Determine whether any of the channels should be replaced by the other. Use
  // hysteresis for going back from channel replacement to avoid toggling back
  // and forth repeatedly.
  const auto should_channel_be_replaced =
      [](bool channel_was_replaced_last_frame,
         bool channel_at_most_somewhat_stronger,
         bool other_channel_is_much_stronger) {
        if (other_channel_is_much_stronger) {
          // Replace the channel content if the other channel contains much
          // stronger audio.
          return true;
        }
        if (channel_was_replaced_last_frame &&
            channel_at_most_somewhat_stronger) {
          // Keep replace the channel content if its audio is at most somewhat
          // stronger than the audio in the other channel.
          return true;
        }
        return false;
      };

  replace_channel_0 = should_channel_be_replaced(
      channel_0_replaced_last_frame, channel_0_at_most_somewhat_stronger,
      channel_1_much_stronger);

  replace_channel_1 = should_channel_be_replaced(
      channel_1_replaced_last_frame, channel_1_at_most_somewhat_stronger,
      channel_0_much_stronger);

  RTC_DCHECK(!(replace_channel_0 && replace_channel_1));
}

// Analyzes the channel audio energies in `average_energy` to determine whether
// any of the channels should be replaced. channel_0_replaced_last_fram and
// channel_1_replaced_last_frame indicate whether either of the channels was
// replaced during the previous frame. If identified, the channel to replace is
// returned, but if no channel should be replaced nullopt is returned. The
// method requires that only two channels are present.
absl::optional<int> DetermineChannelReplacement(
    rtc::ArrayView<const float> average_energy,
    bool channel_0_replaced_last_frame,
    bool channel_1_replaced_last_frame) {
  RTC_DCHECK_EQ(average_energy.size(), 2);
  RTC_DCHECK(!(channel_0_replaced_last_frame && channel_1_replaced_last_frame));

  bool channel_0_much_stronger;
  bool channel_1_much_stronger;
  bool channel_0_at_most_somewhat_stronger;
  bool channel_1_at_most_somewhat_stronger;
  CompareStereoChannelsStrengths(
      average_energy, channel_0_much_stronger, channel_1_much_stronger,
      channel_0_at_most_somewhat_stronger, channel_1_at_most_somewhat_stronger);

  bool replace_channel_0;
  bool replace_channel_1;
  MakeReplacementDecision(
      channel_0_replaced_last_frame, channel_1_replaced_last_frame,
      channel_0_much_stronger, channel_1_much_stronger,
      channel_0_at_most_somewhat_stronger, channel_1_at_most_somewhat_stronger,
      replace_channel_0, replace_channel_1);

  // Produce return value.
  if (replace_channel_0) {
    RTC_DCHECK(!replace_channel_1);
    return 0;
  }
  if (replace_channel_1) {
    return 1;
  }
  return absl::nullopt;
}

// Creates a fake-stereo signal (same content in both channels) by replacing both channels in `audio_buffer` with a downmixed version. The method requires that `audio_buffer` has 2 channels.
void FormFakeStereo(AudioBuffer& audio_buffer) {
  RTC_DCHECK_EQ(audio_buffer.num_channels(), 2);
  rtc::ArrayView< float> left_channel_data(
      &audio_buffer.channels()[0][0],
      audio_buffer.num_frames());
  rtc::ArrayView<float> right_channel_data(
      &audio_buffer.channels()[1][0],
      audio_buffer.num_frames());

  for (int k = 0; k < static_cast<int>(left_channel_data.size()); ++k) {
    const float mono_sample = (left_channel_data[k] + right_channel_data[k]) * 0.5f;
    left_channel_data[k]  =mono_sample;
    right_channel_data[k]=mono_sample;
  }
}

}  // namespace

MultiChannelContentAdjuster::MultiChannelContentAdjuster()
    : channel_0_content_replacer_(0),
      channel_1_content_replacer_(/*channel_to_replace=*/1) {}

void MultiChannelContentAdjuster::HandleUnsuitableMicChannels(
    AudioBuffer& audio_buffer) {
  RTC_DCHECK(!audio_buffer.IsBandSplit());
  ReactToAudioFormatChanges(audio_buffer);

  // Only handle stereo content since there is nothing to do for mono content
  // and the output of content and behavior beyond stereo tend to be
  // setup-specific.
  if (num_channels_ != 2) {
    return;
  }

  const bool reliable_estimates = audio_content_analyzer_.Analyze(audio_buffer);

  if (!reliable_estimates) {
    // Downmix to mono (fake-stereo content with the same channel content) until reliable estimates have been achieved.
    FormFakeStereo(audio_buffer);
    return;
  }

  // Retrieve audio channel energy metric.
  const std::vector<float>& average_energy =
      audio_content_analyzer_.GetChannelEnergies();

  // Find out which, if any, channel that is to be replaced.
  absl::optional<int> channel_to_replace = DetermineChannelReplacement(
      average_energy, channel_0_replaced_last_frame_,
      channel_1_replaced_last_frame_);

  // Optionally replace channel 0.
  if ((channel_to_replace && *channel_to_replace == 0) ||
      channel_0_replaced_last_frame_) {
    const int channel_to_replace_from =
        channel_to_replace ? *channel_to_replace : 0;
    channel_0_content_replacer_.ReplaceChannelContent(channel_to_replace_from,
                                                      audio_buffer);
    channel_0_replaced_last_frame_ = channel_to_replace_from != 0;
  }
  // Optionally replace channel 0.
  if ((channel_to_replace && *channel_to_replace == 1) ||
      channel_1_replaced_last_frame_) {
    const int channel_to_replace_from =
        channel_to_replace ? *channel_to_replace : 1;
    channel_1_content_replacer_.ReplaceChannelContent(channel_to_replace_from,
                                                      audio_buffer);
    channel_1_replaced_last_frame_ = channel_to_replace_from != 1;
  }
}

void MultiChannelContentAdjuster::Reset() {
  channel_0_replaced_last_frame_ = false;
  channel_1_replaced_last_frame_ = false;

  audio_content_analyzer_.Reset();
  channel_0_content_replacer_.Reset();
  channel_1_content_replacer_.Reset();
}

void MultiChannelContentAdjuster::ReactToAudioFormatChanges(
    const AudioBuffer& audio_buffer) {
  if (static_cast<int>(audio_buffer.num_channels()) == num_channels_ &&
      static_cast<int>(audio_buffer.num_frames()) == num_samples_per_channel_) {
    return;
  }

  audio_content_analyzer_.SetAudioProperties(audio_buffer);

  num_channels_ = audio_buffer.num_channels();
  num_samples_per_channel_ = audio_buffer.num_frames();
  one_by_num_samples_per_channel_ = 1.0f / num_samples_per_channel_;

  // Reset audio content analysis and assessments when the audio format changes.
  Reset();
}

}  // namespace webrtc
