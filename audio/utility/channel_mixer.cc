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

  if (frame->muted()) {
    frame->num_channels_ = output_channels_;
    return;
  }

  // All mixing is done in-place.
  const int16_t* in_audio = frame->data();
  int16_t* out_audio = frame->mutable_data();

  // If we're just remapping we know that each output channel gets contribution
  // from one or less input channels. Hence, all we have to here is to look for
  // one input channel with a scaling factor of one and map the contribution
  // from that input to the corresponding output. If no such scaling factor is
  // found, the output shall be set to zero.
  if (remapping_) {
    RTC_DCHECK(UpMixing());
    // Up-mixing done in-place. Going backwards through the frame ensures that
    // nothing is irrevocably overwritten.
    for (int i = frame->samples_per_channel() - 1; i >= 0; i--) {
      for (int output_ch = 0; output_ch < output_channels_; ++output_ch) {
        bool in_mapped_to_out = false;
        for (int input_ch = 0; input_ch < input_channels_; ++input_ch) {
          float scale = matrix_[output_ch][input_ch];
          if (scale > 0) {
            // Only one input channel can contribute to the output of any given
            // output channel. Hence, break after copying the input value to the
            // output.
            RTC_DCHECK_EQ(scale, 1.0f);
            in_mapped_to_out = true;
            out_audio[output_channels_ * i + output_ch] =
                in_audio[input_channels_ * i + input_ch];
            break;
          }
        }
        // All matrix coefficients were zero, hence set output to zero.
        if (!in_mapped_to_out) {
          out_audio[output_channels_ * i + output_ch] = 0;
        }
      }
    }

    frame->num_channels_ = output_channels_;
    return;
  }

  // Reduce the number of channels by creating a weighted sum of input samples
  // where the weights (scale factors) for each output sample are given by the
  // transformation matrix.
  RTC_DCHECK(DownMixing());
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
      *out_audio = rtc::saturated_cast<int16_t>(acc_value);
      out_audio++;
    }
  }

  frame->num_channels_ = output_channels_;
}

}  // namespace webrtc
