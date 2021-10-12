/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/gain_applier.h"

#include <math.h>

#include <algorithm>
#include <limits>

#include "modules/audio_processing/agc2/vector_float_frame.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

constexpr int kMono = 1;
constexpr int kSampleRateHz = 8000;
constexpr bool kHardClip = true;
constexpr bool kNoHardClip = false;

// TODO(bugs.webrtc.org/7494): Parametrized test: # channels, sample rate.

// Checks that the gain specified when `GainApplier` is constructed is applied.
TEST(AutomaticGainController2GainApplier, InitialGainIsApplied) {
  constexpr float kInitialSignalLevel = 123.0f;
  constexpr float kGainFactor = 10.0f;
  VectorFloatFrame audio(kMono, kSampleRateHz, kInitialSignalLevel);
  GainApplier gain_applier(kGainFactor, kHardClip, kSampleRateHz);
  gain_applier.ApplyGain(audio.float_frame_view());
  EXPECT_NEAR(audio.float_frame_view().channel(0)[0],
              kInitialSignalLevel * kGainFactor, 0.1f);
}

TEST(AutomaticGainController2GainApplier, ClippingIsDone) {
  constexpr float kInitialSignalLevel = 30000.0f;
  constexpr float kGainFactor = 10.0f;
  VectorFloatFrame audio(kMono, kSampleRateHz, kInitialSignalLevel);
  GainApplier gain_applier(kGainFactor, kHardClip, kSampleRateHz);

  gain_applier.ApplyGain(audio.float_frame_view());
  EXPECT_NEAR(audio.float_frame_view().channel(0)[0],
              std::numeric_limits<int16_t>::max(), 0.1f);
}

TEST(AutomaticGainController2GainApplier, ClippingIsNotDone) {
  constexpr float kInitialSignalLevel = 30000.0f;
  constexpr float kGainFactor = 10.f;
  VectorFloatFrame audio(kMono, kSampleRateHz, kInitialSignalLevel);
  GainApplier gain_applier(kGainFactor, kNoHardClip, kSampleRateHz);

  gain_applier.ApplyGain(audio.float_frame_view());

  EXPECT_NEAR(audio.float_frame_view().channel(0)[0],
              kInitialSignalLevel * kGainFactor, 0.1f);
}

TEST(AutomaticGainController2GainApplier, RampingIsDone) {
  constexpr float kInitialSignalLevel = 30000.0f;
  constexpr float kInitialGainFactor = 1.0f;
  constexpr float kTargetGainFactor = 0.5f;
  constexpr int kNumChannels = 3;
  VectorFloatFrame audio(kNumChannels, kSampleRateHz, kInitialSignalLevel);
  GainApplier gain_applier(kInitialGainFactor, kNoHardClip, kSampleRateHz);

  gain_applier.SetGainFactor(kTargetGainFactor);
  gain_applier.ApplyGain(audio.float_frame_view());

  // The maximal gain change should be close to that in linear interpolation.
  for (size_t channel = 0; channel < kNumChannels; ++channel) {
    float max_signal_change = 0.0f;
    float last_signal_level = kInitialSignalLevel;
    for (const auto sample : audio.float_frame_view().channel(channel)) {
      const float current_change = fabs(last_signal_level - sample);
      max_signal_change = std::max(max_signal_change, current_change);
      last_signal_level = sample;
    }
    const float total_gain_change =
        fabs((kInitialGainFactor - kTargetGainFactor) * kInitialSignalLevel);
    EXPECT_NEAR(max_signal_change, total_gain_change / kSampleRateHz, 0.1f);
  }

  // Next frame should have the desired level.
  VectorFloatFrame next_audio_frame(kNumChannels, kSampleRateHz,
                                    kInitialSignalLevel);
  gain_applier.ApplyGain(next_audio_frame.float_frame_view());

  // The last sample should have the new gain.
  EXPECT_NEAR(next_audio_frame.float_frame_view().channel(0)[0],
              kInitialSignalLevel * kTargetGainFactor, 0.1f);
}

}  // namespace
}  // namespace webrtc
