/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_channels_selector/mono_channel_selector.h"

#include <math.h>

#include <algorithm>
#include <limits>

#include "api/array_view.h"
#include "modules/audio_processing/audio_buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

// Identifies and returns any channel that permanently can be selected. If no
// such channel is available, nullopt is retured. A channel is selected if its
// content is sufficiently strong. The selection is intentionally biased towards
// choosing channel 0, as that well matches the legacy behavior for the channel
// selection. The selection is based on the values of max_abs_samples and
// average_energy for the different channels, and their corresponding indices
// `max_abs_sample_channel` and `max_energy_channel` for their maximum elements.
absl::optional<float> IdentifyChannelForPermamentSelection(
    rtc::ArrayView<const float> max_abs_samples,
    int max_abs_sample_channel,
    rtc::ArrayView<const float> average_energy,
    int max_energy_channel) {
  constexpr float kDominantChannelThreshold = 10.0f;
  constexpr float kWeakSignalThreshold = 100.0f;

  const bool channel_0_sufficiently_strong =
      max_abs_samples[0] > kWeakSignalThreshold;
  const bool channel_0_not_much_weaker =
      max_energy_channel == 0 ||
      average_energy[max_energy_channel] <=
          kDominantChannelThreshold * average_energy[0];

  const bool max_energy_channel_sufficiently_strong =
      max_abs_samples[max_energy_channel] > kWeakSignalThreshold;
  const bool max_energy_channel_dominant =
      average_energy[max_energy_channel] >
      kDominantChannelThreshold * average_energy[0];

  if (channel_0_sufficiently_strong && channel_0_not_much_weaker) {
    // Choose channel 0 if it is sufficiently strong, and not much weaker
    // compared to the strongest channel.
    return 0;
  }

  if (max_energy_channel_sufficiently_strong && max_energy_channel_dominant) {
    // Choose the strongest channel if it is sufficiently strong, and not
    // much weaker compared to the strongest channel.
    return max_energy_channel;
  }

  return absl::nullopt;
}

// Identifies and returns the channel that should temporarily be used based on
// the observed maximum absolute sample values in each channel (in
// `max_abs_samples`), the index of the channel where those are highest, and the
// channel `previously_selected_channel` that was previously selected. The
// selection is done such a strong channels is selected but such that the
// channel selection should not vary too much over time.
int IdentifyChannelForTemporarySelection(
    rtc::ArrayView<const float> max_abs_samples,
    int max_abs_sample_channel,
    int previously_selected_channel) {
  constexpr float kSilentChannelThreshold = 10.0f;
  const bool audio_is_silent =
      max_abs_samples[max_abs_sample_channel] < kSilentChannelThreshold;

  // Check metrics indicating how much stronger the strongest channel is
  // compared to the previously selected channel.
  constexpr float kDominantThreshold = 4.0f;
  const bool dominant_max_channel =
      max_abs_samples[max_abs_sample_channel] >
      kDominantThreshold * max_abs_samples[previously_selected_channel];

  return dominant_max_channel && !audio_is_silent ? max_abs_sample_channel
                                                  : previously_selected_channel;
}

// Retrieves index for the maximum element of `v`.
int GetMaxVectorIndex(const std::vector<float>& v) {
  std::vector<const float>::iterator max_iterator =
      std::max_element(v.begin(), v.end());
  return std::distance(v.begin(), max_iterator);
}

constexpr int kDefaultSelectedChannel = 0;

}  // namespace

MonoChannelSelector::MonoChannelSelector()
    : channel_content_replacer_(/*channel_to_replace=*/0),
      previously_selected_channel_(kDefaultSelectedChannel) {}

void MonoChannelSelector::DownMixToBestChannel(AudioBuffer& audio_buffer) {
  printf("a0-------------------\n");
  RTC_DCHECK(!audio_buffer.IsBandSplit());
  ReactToAudioFormatChanges(audio_buffer);
      printf("a1-------------------\n");

  // There is nothing to do for mono content.
  if (num_channels_ == 1) {
    printf("a2-------------------\n");
    return;
  }

  printf("a3-------------------\n");
  const bool reliable_estimates = audio_content_analyzer_.Analyze(audio_buffer);
  printf("a4-------------------\n");
  ++num_frames_analyzed_;
  int channel_to_use;
  if (permanently_selected_channel_) {
    channel_to_use = *permanently_selected_channel_;
  } else {
    // Retrieve audio channel energy and sample metrics.
    const std::vector<float>& max_abs_samples =
        audio_content_analyzer_.GetMaxAbsSampleInChannels();
    const std::vector<float>& average_energy =
        audio_content_analyzer_.GetChannelEnergies();
    RTC_DCHECK_EQ(average_energy.size(), num_channels_);
    RTC_DCHECK_EQ(max_abs_samples.size(), num_channels_);
    printf("a-------------------\n");

    // Identify the channels with the strongest signal in terms of max abs
    // sample values and energy content.
    const int max_abs_sample_channel = GetMaxVectorIndex(max_abs_samples);
    const int max_energy_channel = GetMaxVectorIndex(average_energy);

    constexpr int kNumFramesToAnalyzeBeforeReliableEstimates = 50;
    if (num_frames_analyzed_ > kNumFramesToAnalyzeBeforeReliableEstimates &&
        reliable_estimates) {
      RTC_DCHECK(!permanently_selected_channel_);
      permanently_selected_channel_ = IdentifyChannelForPermamentSelection(
          max_abs_samples, max_abs_sample_channel, average_energy,
          max_energy_channel);
    }

        printf("b-------------------\n");
    if (permanently_selected_channel_) {
      channel_to_use = *permanently_selected_channel_;
    } else {
      channel_to_use = IdentifyChannelForTemporarySelection(
          max_abs_samples, max_abs_sample_channel,
          previously_selected_channel_);
    }
    printf("c-------------------\n");
  }

      printf("d-------------------\n");
  if (channel_to_use != 0 || previously_selected_channel_ != 0) {
    channel_content_replacer_.ReplaceChannelContent(channel_to_use,
                                                    audio_buffer);
  }
  printf("e-------------------\n");
  previously_selected_channel_ = channel_to_use;

  audio_buffer.set_num_channels(1);
  printf("f-------------------\n");
}

void MonoChannelSelector::Reset() {
  num_frames_analyzed_ = 0;
  permanently_selected_channel_ = absl::nullopt;
  previously_selected_channel_ = kDefaultSelectedChannel;

  audio_content_analyzer_.Reset();
  channel_content_replacer_.Reset();
}

void MonoChannelSelector::ReactToAudioFormatChanges(
    const AudioBuffer& audio_buffer) {
  if (static_cast<int>(audio_buffer.num_channels()) == num_channels_ &&
      static_cast<int>(audio_buffer.num_frames()) == num_samples_per_channel_) {
    return;
  }

  audio_content_analyzer_.SetAudioProperties(audio_buffer);
  channel_content_replacer_.SetAudioProperties(audio_buffer);

  num_channels_ = audio_buffer.num_channels();
  num_samples_per_channel_ = audio_buffer.num_frames();

  // Reset audio content analysis and assessments when the number of channels
  // change.
  Reset();
}

}  // namespace webrtc
