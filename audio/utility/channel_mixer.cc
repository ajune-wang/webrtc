/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/utility/channel_mixer.h"

#include <stddef.h>

#include "api/audio/audio_frame.h"
#include "audio/utility/channel_mixing_matrix.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

ChannelMixer::ChannelMixer(ChannelLayout input_layout,
                           ChannelLayout output_layout)
    : input_layout_(input_layout), output_layout_(output_layout) {
  Initialize(input_layout, ChannelLayoutToChannelCount(input_layout),
             output_layout, ChannelLayoutToChannelCount(output_layout));
}

void ChannelMixer::Initialize(ChannelLayout input_layout,
                              int input_channels,
                              ChannelLayout output_layout,
                              int output_channels) {
  input_channels_ = input_channels;
  output_channels_ = output_channels;

  // Create the transformation matrix.
  ChannelMixingMatrix matrix_builder(input_layout, input_channels,
                                     output_layout, output_channels);
  remapping_ = matrix_builder.CreateTransformationMatrix(&matrix_);
}

ChannelMixer::~ChannelMixer() = default;

void ChannelMixer::Transform(AudioFrame* frame) {
  RTC_DCHECK(frame);
  RTC_DCHECK_EQ(matrix_[0].size(), static_cast<size_t>(input_channels_));
  RTC_DCHECK_EQ(matrix_.size(), static_cast<size_t>(output_channels_));
  if (UpMixing()) {
    RTC_CHECK_LE(output_channels_ * frame->samples_per_channel(),
                 frame->max_16bit_samples());
  }

  // Leave the audio frame intact if the channel layouts for in and out are
  // identical.
  if (input_layout_ == output_layout_) {
    return;
  }

  // Only change the number of output channels if the audio frame is muted.
  if (frame->muted()) {
    frame->num_channels_ = output_channels_;
    return;
  }

  // Down-mixing is done in-place while up-mixing requires a extra buffer and
  // an extra copy operation.
  const int16_t* in_audio = frame->data();
  int16_t* out_audio = frame->mutable_data();

  // Allocate extra buffer space when up-mixing and use this area for storage
  // of output samples to avoid overlap which can happen if mixing is performed
  // in-place.
  // TODO(henrika): it might be possible to do in-place up-mixing but I was not
  // able to solve all possible cases without overwriting input samples.
  // E.g. CHANNEL_LAYOUT_2_1 -> CHANNEL_LAYOUT_2_2 failed.
  if (UpMixing()) {
    out_vec_.reserve(frame->samples_per_channel() * output_channels_);
    out_audio = out_vec_.data();
  }

  // Modify the number of channels by creating a weighted sum of input samples
  // where the weights (scale factors) for each output sample are given by the
  // transformation matrix.
  for (size_t i = 0; i < frame->samples_per_channel(); i++) {
    for (int output_ch = 0; output_ch < output_channels_; ++output_ch) {
      float acc_value = 0.0f;
      for (int input_ch = 0; input_ch < input_channels_; ++input_ch) {
        float scale = matrix_[output_ch][input_ch];
        // Scale should always be positive.  Don't bother scaling by zero.
        RTC_DCHECK_GE(scale, 0);
        if (scale > 0) {
          // Each output sample is a weighted sum of input samples.
          acc_value += scale * in_audio[i * input_channels_ + input_ch];
        }
      }
      out_audio[output_channels_ * i + output_ch] =
          rtc::saturated_cast<int16_t>(acc_value);
    }
  }

  frame->num_channels_ = output_channels_;

  // Copy the output result to the audio frame in |frame| but only when we are
  // up-mixing. Down-mixing is done in-place and does not need this step.
  if (UpMixing()) {
    int16_t* frame_data = frame->mutable_data();
    for (size_t i = 0; i < frame->samples_per_channel() * frame->channels();
         i++) {
      frame_data[i] = out_audio[i];
    }
  }
}

}  // namespace webrtc
