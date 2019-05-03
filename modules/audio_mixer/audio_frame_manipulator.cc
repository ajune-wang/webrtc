/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_mixer/audio_frame_manipulator.h"

#if defined(__clang__) && (defined(__aarch64__) || defined(__ARMEL__))
#include <arm_neon.h>
#endif

#include "audio/utility/audio_frame_operations.h"
#include "rtc_base/checks.h"

namespace webrtc {

uint32_t AudioMixerCalculateEnergy(const AudioFrame& audio_frame) {
  if (audio_frame.muted()) {
    return 0;
  }

  uint32_t energy = 0;
  const int16_t* frame_data = audio_frame.data();

#if defined(__clang__) && (defined(__aarch64__) || defined(__ARMEL__))
  const int16_t* p = frame_data;
  const int16_t* end =
      frame_data + audio_frame.samples_per_channel_ * audio_frame.num_channels_;
  int32x4_t sumvec = vdupq_n_s32(0);
  int16x4_t vec4;
#pragma unroll 4
  for (; p + 4 < end; p += 4) {
    vec4 = vld1_s16(p);
    sumvec = vmlal_s16(sumvec, vec4, vec4);
  }

  int32x2_t r = vadd_s32(vget_high_s32(sumvec), vget_low_s32(sumvec));

  energy = (uint32_t)vget_lane_s32(vpadd_s32(r, r), 0);
  while (p < end) {
    energy += (int32_t)(*p) * (int32_t)(*p);
    ++p;
  }
#else
#ifdef __clang__
#pragma unroll 4
#elif defined(__GNUC__) && __GNUC__ > 8
#pragma GCC unroll 8
#endif
  for (size_t position = 0;
       position < audio_frame.samples_per_channel_ * audio_frame.num_channels_;
       position++) {
    // TODO(aleloi): This can overflow. Convert to floats.
    energy += (int32_t)frame_data[position] * (int32_t)frame_data[position];
  }
#endif
  return energy;
}

void Ramp(float start_gain, float target_gain, AudioFrame* audio_frame) {
  RTC_DCHECK(audio_frame);
  RTC_DCHECK_GE(start_gain, 0.0f);
  RTC_DCHECK_GE(target_gain, 0.0f);
  if (start_gain == target_gain || audio_frame->muted()) {
    return;
  }

  size_t samples = audio_frame->samples_per_channel_;
  RTC_DCHECK_LT(0, samples);
  float increment = (target_gain - start_gain) / samples;
  float gain = start_gain;
  int16_t* frame_data = audio_frame->mutable_data();
  for (size_t i = 0; i < samples; ++i) {
    // If the audio is interleaved of several channels, we want to
    // apply the same gain change to the ith sample of every channel.
    for (size_t ch = 0; ch < audio_frame->num_channels_; ++ch) {
      frame_data[audio_frame->num_channels_ * i + ch] *= gain;
    }
    gain += increment;
  }
}

void RemixFrame(size_t target_number_of_channels, AudioFrame* frame) {
  RTC_DCHECK_GE(target_number_of_channels, 1);
  if (frame->num_channels_ == target_number_of_channels) {
    return;
  }
  if (frame->num_channels_ > target_number_of_channels) {
    AudioFrameOperations::DownmixChannels(target_number_of_channels, frame);
  } else if (frame->num_channels_ < target_number_of_channels) {
    AudioFrameOperations::UpmixChannels(target_number_of_channels, frame);
  }
  RTC_DCHECK_EQ(frame->num_channels_, target_number_of_channels)
      << "Wrong number of channels, " << frame->num_channels_ << " vs "
      << target_number_of_channels;
}
}  // namespace webrtc
