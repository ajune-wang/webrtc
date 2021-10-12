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

#include "api/array_view.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

// Minimum supported sample rate.
constexpr int kMinSampleRateHz = 8000;
// Number of samples for a 10 ms frame with `kMinSampleRateHz` as sample rate.
constexpr int kMinFrameSize = kMinSampleRateHz / 100;

// Returns true when the gain factor is so close to 1 that it would not affect
// int16 samples.
bool GainCloseToOne(float gain_factor) {
  constexpr float kInverseOfMaxS16 = 1.0f / kMaxFloatS16Value;
  constexpr float kMinGain = 1.0f - kInverseOfMaxS16;
  constexpr float kMaxGain = 1.0f + kInverseOfMaxS16;
  return kMinGain <= gain_factor && gain_factor <= kMaxGain;
}

void ClipSignal(AudioFrameView<float> signal) {
  for (int k = 0; k < signal.num_channels(); ++k) {
    rtc::ArrayView<float> channel_view = signal.channel(k);
    for (auto& sample : channel_view) {
      sample = rtc::SafeClamp(sample, kMinFloatS16Value, kMaxFloatS16Value);
    }
  }
}

void ApplyGainWithRamping(float last_gain_linear,
                          float gain_at_end_of_frame_linear,
                          float inverse_samples_per_channel,
                          AudioFrameView<float> float_frame) {
  RTC_DCHECK_GT(inverse_samples_per_channel, 0.0f);
  RTC_DCHECK_LE(inverse_samples_per_channel, 1.0f / kMinFrameSize);
  // Do not modify the signal.
  if (last_gain_linear == gain_at_end_of_frame_linear &&
      GainCloseToOne(gain_at_end_of_frame_linear)) {
    return;
  }

  // Gain is constant and different from 1.
  if (last_gain_linear == gain_at_end_of_frame_linear) {
    for (int k = 0; k < float_frame.num_channels(); ++k) {
      rtc::ArrayView<float> channel_view = float_frame.channel(k);
      for (auto& sample : channel_view) {
        sample *= gain_at_end_of_frame_linear;
      }
    }
    return;
  }

  // The gain changes. We have to change slowly to avoid discontinuities.
  const float increment = (gain_at_end_of_frame_linear - last_gain_linear) *
                          inverse_samples_per_channel;
  float gain = last_gain_linear;
  for (int i = 0; i < float_frame.samples_per_channel(); ++i) {
    for (int ch = 0; ch < float_frame.num_channels(); ++ch) {
      float_frame.channel(ch)[i] *= gain;
    }
    gain += increment;
  }
}

}  // namespace

GainApplier::GainApplier(float gain_factor, bool hard_clip, int sample_rate_hz)
    : hard_clip_(hard_clip),
      last_gain_factor_(gain_factor),
      current_gain_factor_(gain_factor),
      inverse_samples_per_channel_(0.0f) {
  Initialize(sample_rate_hz);
}

void GainApplier::Initialize(int sample_rate_hz) {
  RTC_DCHECK_GE(sample_rate_hz, kMinSampleRateHz);
  int samples_per_channel = rtc::CheckedDivExact(sample_rate_hz, 100);
  inverse_samples_per_channel_ = 1.0f / samples_per_channel;
}

void GainApplier::ApplyGain(AudioFrameView<float> signal) {
  ApplyGainWithRamping(last_gain_factor_, current_gain_factor_,
                       inverse_samples_per_channel_, signal);
  last_gain_factor_ = current_gain_factor_;
  if (hard_clip_) {
    ClipSignal(signal);
  }
}

void GainApplier::SetGainFactor(float gain_factor) {
  RTC_DCHECK_GT(gain_factor, 0.0f);
  current_gain_factor_ = gain_factor;
}


}  // namespace webrtc
