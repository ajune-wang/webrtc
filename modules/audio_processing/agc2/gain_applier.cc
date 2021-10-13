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
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

// Minimum supported sample rate.
constexpr int kMinSampleRateHz = 8000;
// Number of samples for a 10 ms frame with `kMinSampleRateHz` as sample rate.
constexpr int kMinFrameSize = kMinSampleRateHz / 100;

void ClipSignal(AudioFrameView<float> frame) {
  for (int k = 0; k < frame.num_channels(); ++k) {
    rtc::ArrayView<float> channel_view = frame.channel(k);
    for (auto& sample : channel_view) {
      sample = rtc::SafeClamp(sample, kMinFloatS16Value, kMaxFloatS16Value);
    }
  }
}

}  // namespace

GainApplier::GainApplier(float gain_db, bool hard_clip, int sample_rate_hz)
    : hard_clip_(hard_clip) {
  SetGainDb(gain_db);
  Initialize(sample_rate_hz);
  last_gain_factor_ = current_gain_factor_;
}

void GainApplier::Initialize(int sample_rate_hz) {
  RTC_DCHECK_GE(sample_rate_hz, kMinSampleRateHz);
  int samples_per_channel = rtc::CheckedDivExact(sample_rate_hz, 100);
  inverse_samples_per_channel_ = 1.0f / samples_per_channel;
}

void GainApplier::SetGainDb(float gain_db) {
  gain_db_ = gain_db;
  current_gain_factor_ = DbToRatio(gain_db_);
  RTC_DCHECK_GT(current_gain_factor_, 0.0f);
  // Do not apply the gain if its application has no effect on S16 samples.
  constexpr float kInverseOfMaxS16 = 1.0f / kMaxFloatS16Value;
  constexpr float kMinGain = 1.0f - kInverseOfMaxS16;
  constexpr float kMaxGain = 1.0f + kInverseOfMaxS16;
  apply_gain_ =
      current_gain_factor_ < kMinGain || current_gain_factor_ > kMaxGain;
}

void GainApplier::ApplyGain(AudioFrameView<float> frame) {
  const bool gain_changed = last_gain_factor_ != current_gain_factor_;
  if (!gain_changed && apply_gain_) {
    // Apply the gain, which is not ramping down/up.
    for (int c = 0; c < frame.num_channels(); ++c) {
      rtc::ArrayView<float> channel_view = frame.channel(c);
      for (auto& sample : channel_view) {
        sample *= current_gain_factor_;
      }
    }
  } else if (gain_changed) {
    // Apply the gain even if `apply_gain_` is false since the gain must ramp
    // down/up to avoid discontinuities.
    RTC_DCHECK_GT(inverse_samples_per_channel_, 0.0f);
    RTC_DCHECK_LE(inverse_samples_per_channel_, 1.0f / kMinFrameSize);
    const float increment = (current_gain_factor_ - last_gain_factor_) *
                            inverse_samples_per_channel_;
    float gain = last_gain_factor_;
    for (int i = 0; i < frame.samples_per_channel(); ++i) {
      for (int ch = 0; ch < frame.num_channels(); ++ch) {
        frame.channel(ch)[i] *= gain;
      }
      gain = std::min(gain + increment, current_gain_factor_);
    }
    last_gain_factor_ = current_gain_factor_;
  }
  if (hard_clip_) {
    ClipSignal(frame);
  }
}

}  // namespace webrtc
