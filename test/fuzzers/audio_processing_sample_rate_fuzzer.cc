/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "api/audio/audio_frame.h"
#include "modules/audio_processing/include/audio_frame_proxies.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
namespace {

void GenerateFloatFrame(test::FuzzDataHelper& fuzz_data,
                        int input_rate,
                        int num_channels,
                        bool is_capture,
                        float* const* float_frames) {
  const int samples_per_input_channel = input_rate / 100;
  RTC_DCHECK_LE(samples_per_input_channel, 3840);
  for (int i = 0; i < num_channels; ++i) {
    float channel_value;
    fuzz_data.CopyTo<float>(&channel_value);
    std::fill(float_frames[i], float_frames[i] + samples_per_input_channel,
              channel_value);
  }
}

void GenerateFixedFrame(test::FuzzDataHelper& fuzz_data,
                        int input_rate,
                        int num_channels,
                        AudioFrame& fixed_frame) {
  const int samples_per_input_channel = input_rate / 100;
  fixed_frame.samples_per_channel_ = samples_per_input_channel;
  fixed_frame.sample_rate_hz_ = input_rate;
  fixed_frame.num_channels_ = num_channels;
  RTC_DCHECK_LE(samples_per_input_channel * num_channels,
                AudioFrame::kMaxDataSizeSamples);

  for (int ch = 0; ch < num_channels; ++ch) {
    const float channel_value = fuzz_data.ReadOrDefaultValue<int16_t>(0);
    for (int i = ch; i < samples_per_input_channel * num_channels;
         i += num_channels) {
      fixed_frame.mutable_data()[i] = channel_value;
    }
  }
}
}  // namespace

// This fuzzer is directed at fuzzing unexpected input and output sample rates
// of APM. For example, the sample rate 22050 Hz is processed by APM in frames
// of floor(22050/100) = 220 samples. This is not exactly 10 ms of audio
// content, and may break assumptions commonly made on the APM frame size.
void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 100) {
    return;
  }
  test::FuzzDataHelper fuzz_data(rtc::ArrayView<const uint8_t>(data, size));

  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetConfig({.pipeline = {.multi_channel_render = true,
                                   .multi_channel_capture = true}})
          .Create();
  RTC_DCHECK(apm);

  AudioFrame fixed_frame;
  constexpr int kMaxNumChannels = 2;
  std::array<std::array<float, 3840>, kMaxNumChannels> float_frames;
  std::array<float*, kMaxNumChannels> float_frame_ptrs;
  for (int i = 0; i < kMaxNumChannels; ++i) {
    float_frame_ptrs[i] = float_frames[i].data();
  }
  float* const* ptr_to_float_frames = &float_frame_ptrs[0];

  // These are all the sample rates logged by UMA metric
  // WebAudio.AudioContext.HardwareSampleRate.
  constexpr int kSampleRatesHz[] = {8000,  11025,  16000,  22050,  24000,
                                    32000, 44100,  46875,  48000,  88200,
                                    96000, 176400, 192000, 352800, 384000};

  // We may run out of fuzz data in the middle of a loop iteration. In
  // that case, default values will be used for the rest of that
  // iteration.
  while (fuzz_data.CanReadBytes(1)) {
    const bool is_float = fuzz_data.ReadOrDefaultValue(true);
    // Decide input/output rate for this iteration.
    const int input_rate = fuzz_data.SelectOneOf(kSampleRatesHz);
    const int output_rate = fuzz_data.SelectOneOf(kSampleRatesHz);
    const int num_channels = fuzz_data.ReadOrDefaultValue(true) ? 2 : 1;

    // Make the APM call depending on capture/render mode and float /
    // fix interface.
    const bool is_capture = fuzz_data.ReadOrDefaultValue(true);

    // Fill the arrays with audio samples from the data.
    int apm_return_code = AudioProcessing::Error::kNoError;
    if (is_float) {
      GenerateFloatFrame(fuzz_data, input_rate, num_channels, is_capture,
                         ptr_to_float_frames);
      if (is_capture) {
        apm_return_code = apm->ProcessStream(
            ptr_to_float_frames, StreamConfig(input_rate, num_channels),
            StreamConfig(output_rate, num_channels), ptr_to_float_frames);
      } else {
        apm_return_code = apm->ProcessReverseStream(
            ptr_to_float_frames, StreamConfig(input_rate, num_channels),
            StreamConfig(output_rate, num_channels), ptr_to_float_frames);
      }
    } else {
      GenerateFixedFrame(fuzz_data, input_rate, num_channels, fixed_frame);

      if (is_capture) {
        apm_return_code = ProcessAudioFrame(apm.get(), &fixed_frame);
      } else {
        apm_return_code = ProcessReverseAudioFrame(apm.get(), &fixed_frame);
      }
    }

    RTC_DCHECK_NE(apm_return_code, AudioProcessing::kBadDataLengthError);
  }
}

}  // namespace webrtc
