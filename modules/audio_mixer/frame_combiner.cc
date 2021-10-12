/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_mixer/frame_combiner.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/rtp_packet_info.h"
#include "api/rtp_packet_infos.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_mixer/audio_frame_manipulator.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

constexpr int kInitialSampleRateHz = 48000;

int GetSamplesPerChannel(int sample_rate_hz) {
  return (sample_rate_hz * webrtc::AudioMixerImpl::kFrameDurationInMs) / 1000;
}

void SetAudioFrameFields(rtc::ArrayView<const AudioFrame* const> mix_list,
                         int num_channels,
                         int sample_rate_hz,
                         int samples_per_channel,
                         AudioFrame* audio_frame_for_mixing) {
  // TODO(bugs.webrtc.org/3390): Set a valid `timestamp`.
  // `timestamp_` is set to the dummy value '0' because it is only supported in
  // the one channel case. The correct value is updated below.
  audio_frame_for_mixing->UpdateFrame(
      /*timestamp=*/0, nullptr, samples_per_channel, sample_rate_hz,
      AudioFrame::kUndefined, AudioFrame::kVadUnknown, num_channels);
  if (mix_list.empty()) {
    audio_frame_for_mixing->elapsed_time_ms_ = -1;
    return;
  }
  audio_frame_for_mixing->timestamp_ = mix_list[0]->timestamp_;
  audio_frame_for_mixing->elapsed_time_ms_ = mix_list[0]->elapsed_time_ms_;
  audio_frame_for_mixing->ntp_time_ms_ = mix_list[0]->ntp_time_ms_;
  std::vector<RtpPacketInfo> packet_infos;
  for (const auto& frame : mix_list) {
    audio_frame_for_mixing->timestamp_ =
        std::min(audio_frame_for_mixing->timestamp_, frame->timestamp_);
    audio_frame_for_mixing->ntp_time_ms_ =
        std::min(audio_frame_for_mixing->ntp_time_ms_, frame->ntp_time_ms_);
    audio_frame_for_mixing->elapsed_time_ms_ = std::max(
        audio_frame_for_mixing->elapsed_time_ms_, frame->elapsed_time_ms_);
    packet_infos.insert(packet_infos.end(), frame->packet_infos_.begin(),
                        frame->packet_infos_.end());
  }
  audio_frame_for_mixing->packet_infos_ =
      RtpPacketInfos(std::move(packet_infos));
}

// Both interleaves and rounds.
void InterleaveToAudioFrame(AudioFrameView<const float> mixing_buffer_view,
                            AudioFrame* audio_frame_for_mixing) {
  const int num_channels = mixing_buffer_view.num_channels();
  const int samples_per_channel = mixing_buffer_view.samples_per_channel();
  int16_t* const mixing_data = audio_frame_for_mixing->mutable_data();
  // Put data in the result frame.
  for (int i = 0; i < num_channels; ++i) {
    for (int j = 0; j < samples_per_channel; ++j) {
      mixing_data[num_channels * j + i] =
          FloatS16ToS16(mixing_buffer_view.channel(i)[j]);
    }
  }
}

void LogMixingStats(rtc::ArrayView<const AudioFrame* const> mix_list,
                    int sample_rate_hz) {
  RTC_HISTOGRAM_COUNTS_100("WebRTC.Audio.AudioMixer.NumIncomingStreams",
                           rtc::dchecked_cast<int>(mix_list.size()));
  RTC_HISTOGRAM_COUNTS_LINEAR(
      "WebRTC.Audio.AudioMixer.NumIncomingActiveStreams2",
      rtc::dchecked_cast<int>(mix_list.size()), /*min=*/1, /*max=*/16,
      /*bucket_count=*/16);
  using NativeRate = AudioProcessing::NativeRate;
  static constexpr NativeRate kNativeRates[] = {
      NativeRate::kSampleRate8kHz, NativeRate::kSampleRate16kHz,
      NativeRate::kSampleRate32kHz, NativeRate::kSampleRate48kHz};
  const auto* rate_position = std::lower_bound(
      std::begin(kNativeRates), std::end(kNativeRates), sample_rate_hz);
  RTC_HISTOGRAM_ENUMERATION(
      "WebRTC.Audio.AudioMixer.MixingRate",
      std::distance(std::begin(kNativeRates), rate_position),
      arraysize(kNativeRates));
}

}  // namespace

constexpr int FrameCombiner::kMaxNumChannels;
constexpr int FrameCombiner::kMaxChannelSize;

FrameCombiner::FrameCombiner(bool use_limiter)
    : use_limiter_(use_limiter),
      sample_rate_hz_(kInitialSampleRateHz),
      samples_per_channel_(GetSamplesPerChannel(kInitialSampleRateHz)),
      data_dumper_(std::make_unique<ApmDataDumper>(0)),
      mixing_buffer_(
          std::make_unique<std::array<std::array<float, kMaxChannelSize>,
                                      kMaxNumChannels>>()),
      limiter_(kInitialSampleRateHz,
               data_dumper_.get(),
               /*histogram_name_prefix=*/"AudioMixer"),
      logging_counter_(0) {
  static_assert(
      kMaxChannelSize * kMaxNumChannels <= AudioFrame::kMaxDataSizeSamples, "");
}

FrameCombiner::~FrameCombiner() = default;

void FrameCombiner::Combine(rtc::ArrayView<AudioFrame* const> mix_list,
                            int num_channels,
                            int sample_rate_hz,
                            AudioFrame* audio_frame_for_mixing) {
  RTC_DCHECK(audio_frame_for_mixing);

  // Detect and handle sample rate changes.
  if (sample_rate_hz_ != sample_rate_hz) {
    sample_rate_hz_ = sample_rate_hz;
    samples_per_channel_ = GetSamplesPerChannel(sample_rate_hz_);
    limiter_.Initialize(sample_rate_hz);
  }
  for (const auto* frame : mix_list) {
    RTC_DCHECK(frame);
    RTC_DCHECK_EQ(sample_rate_hz_, frame->sample_rate_hz_);
    RTC_DCHECK_EQ(samples_per_channel_, frame->samples_per_channel_);
  }

  // Periodically log stats.
  logging_counter_++;
  constexpr int kLoggingPeriodMs = 10000;  // 10 seconds.
  constexpr int kLoggingPeriodNumFrames =
      kLoggingPeriodMs / AudioMixerImpl::kFrameDurationInMs;
  if (logging_counter_ > kLoggingPeriodNumFrames) {
    logging_counter_ = 0;
    LogMixingStats(mix_list, sample_rate_hz_);
  }

  SetAudioFrameFields(mix_list, num_channels, sample_rate_hz_,
                      samples_per_channel_, audio_frame_for_mixing);

  // If there are no streams to mix, mark the mix as muted.
  if (mix_list.empty()) {
    audio_frame_for_mixing->Mute();
    return;
  }

  // Adjust the number of channels for each item in `mix_list`.
  for (auto* frame : mix_list) {
    RemixFrame(/*target_number_of_channels=*/num_channels, frame);
  }

  if (mix_list.size() == 1) {
    // Copy the only available stream into the output mix.
    std::copy(mix_list[0]->data(),
              mix_list[0]->data() + mix_list[0]->num_channels_ *
                                        mix_list[0]->samples_per_channel_,
              audio_frame_for_mixing->mutable_data());
    // No need to apply the limiter with a single stream.
    return;
  }

  Mix(mix_list, num_channels);

  // Create an `AudioFrameView` for `mixing_buffer_`.
  const int output_num_channels = std::min(num_channels, kMaxNumChannels);
  const int output_samples_per_channel =
      std::min(samples_per_channel_, kMaxChannelSize);
  std::array<float*, kMaxNumChannels> channel_pointers{};
  for (int i = 0; i < output_num_channels; ++i) {
    channel_pointers[i] = &(*mixing_buffer_.get())[i][0];
  }
  AudioFrameView<float> mixing_buffer_view(
      &channel_pointers[0], output_num_channels, output_samples_per_channel);
  // Use the limiter if enabled and write the output audio.
  if (use_limiter_) {
    limiter_.Process(mixing_buffer_view);
  }
  InterleaveToAudioFrame(mixing_buffer_view, audio_frame_for_mixing);
}

void FrameCombiner::Mix(rtc::ArrayView<const AudioFrame* const> mix_list,
                        int num_channels) {
  RTC_DCHECK_LE(samples_per_channel_, kMaxChannelSize);
  RTC_DCHECK_LE(num_channels, kMaxNumChannels);
  RTC_DCHECK(mixing_buffer_);
  // Clear the mixing buffer.
  for (auto& channel_buffer : *mixing_buffer_) {
    std::fill(channel_buffer.begin(), channel_buffer.end(), 0.0f);
  }
  // Convert to FloatS16 and mix.
  for (int i = 0; i < rtc::dchecked_cast<int>(mix_list.size()); ++i) {
    const AudioFrame* const frame = mix_list[i];
    const int16_t* const frame_data = frame->data();
    for (int j = 0; j < std::min(num_channels, kMaxNumChannels); ++j) {
      for (int k = 0; k < std::min(samples_per_channel_, kMaxChannelSize);
           ++k) {
        (*mixing_buffer_)[j][k] += frame_data[num_channels * k + j];
      }
    }
  }
}

}  // namespace webrtc
