/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_MIXER_FRAME_COMBINER_H_
#define MODULES_AUDIO_MIXER_FRAME_COMBINER_H_

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/audio/audio_frame.h"
#include "modules/audio_processing/agc2/limiter.h"

namespace webrtc {

class ApmDataDumper;

class FrameCombiner {
 public:
  explicit FrameCombiner(bool use_limiter);
  FrameCombiner(const FrameCombiner&) = delete;
  FrameCombiner& operator=(const FrameCombiner&) = delete;
  ~FrameCombiner();

  // Combines the frames in `mix_list` into `audio_frame_for_mixing`.
  // `num_channels` and `sample_rate_hz` are the desired properties for the
  // mixed audio. If `use_limiter_` is true, a limiter is used to avoid
  // clipping.
  void Combine(rtc::ArrayView<AudioFrame* const> mix_list,
               int num_channels,
               int sample_rate_hz,
               AudioFrame* audio_frame_for_mixing);

 private:
  // Creates a mix of `mix_list` and writes the output audio into
  // `mixing_buffer_`.
  void Mix(rtc::ArrayView<const AudioFrame* const> mix_list, int num_channels);

  // Maximum number of channels supported by the implementation.
  static constexpr int kMaxNumChannels = 8;
  // Maximum sample rate supported by the implementation.
  static constexpr int kSampleRateHz = 48000;
  static constexpr int kMaxChannelSize = kSampleRateHz / 100;  // 10 ms frames.

  const bool use_limiter_;
  int sample_rate_hz_;
  int samples_per_channel_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  std::unique_ptr<
      std::array<std::array<float, kMaxChannelSize>, kMaxNumChannels>>
      mixing_buffer_;
  Limiter limiter_;
  int logging_counter_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_MIXER_FRAME_COMBINER_H_
