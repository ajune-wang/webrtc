/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/acm2/acm_remixing.h"

namespace webrtc {

namespace {}  // namespace

void DownMixFrame(const AudioFrame& frame,
                  size_t length_out_buff,
                  int16_t* out_buff) {
  RTC_DCHECK_EQ(frame.num_channels_, 2);
  RTC_DCHECK_GE(length_out_buff, frame.samples_per_channel_);

  if (!frame.muted()) {
    const int16_t* frame_data = frame.data();
    for (size_t n = 0; n < frame.samples_per_channel_; ++n) {
      out_buff[n] =
          static_cast<int16_t>((static_cast<int32_t>(frame_data[2 * n]) +
                                static_cast<int32_t>(frame_data[2 * n + 1])) >>
                               1);
    }
  } else {
    std::fill(out_buff, out_buff + frame.samples_per_channel_, 0);
  }
}

void ReMixFrame(const AudioFrame& input,
                size_t num_output_channels,
                std::vector<int16_t>* output) {
  const size_t output_size = num_output_channels * input.samples_per_channel_;

  if (output->size() != output_size) {
    output->resize(output_size);
  }

  // For muted frames, fill the frame with zeros.
  if (input.muted()) {
    std::fill(output->begin(), output->end(), 0);
    return;
  }

  // Ensure that the special case of zero input channels is handled correctly
  // (zero samples per channel is already handled correctly in the code below).
  if (input.num_channels_ == 0) {
    return;
  }

  const int16_t* input_data = input.data();
  size_t in_index = 0;
  size_t out_index = 0;

  // When upmixing is needed, duplicate the last channel of the input.
  if (input.num_channels_ < num_output_channels) {
    for (size_t k = 0; k < input.samples_per_channel_; ++k) {
      for (size_t j = 0; j < input.num_channels_; ++j) {
        (*output)[out_index++] = input_data[in_index++];
      }
      RTC_DCHECK_GT(in_index, 0);
      const int16_t value_last_channel = input_data[in_index - 1];
      for (size_t j = input.num_channels_; j < num_output_channels; ++j) {
        (*output)[out_index++] = value_last_channel;
      }
    }
    return;
  }

  // When downmixing is needed, and the input is stereo, average the channels.
  if (input.num_channels_ == 2) {
    for (size_t n = 0; n < input.samples_per_channel_; ++n) {
      (*output)[n] =
          static_cast<int16_t>((static_cast<int32_t>(input_data[2 * n]) +
                                static_cast<int32_t>(input_data[2 * n + 1])) >>
                               1);
    }
    return;
  }

  // When downmixing is needed, and the input is multichannel, drop the surplus
  // channels.
  const size_t num_channels_to_drop = input.num_channels_ - num_output_channels;
  for (size_t k = 0; k < input.samples_per_channel_; ++k) {
    for (size_t j = 0; j < num_output_channels; ++j) {
      (*output)[out_index++] = input_data[in_index++];
    }
    in_index += num_channels_to_drop;
  }
}

}  // namespace webrtc
