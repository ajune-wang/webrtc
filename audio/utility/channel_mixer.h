/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_UTILITY_CHANNEL_MIXER_H_
#define AUDIO_UTILITY_CHANNEL_MIXER_H_

#include <vector>

#include "api/audio/channel_layout.h"

namespace webrtc {

class AudioFrame;

// ChannelMixer is for converting audio between channel layouts.  The conversion
// matrix is built upon construction and used during each Transform() call.  The
// algorithm works by generating a conversion matrix mapping each output channel
// to list of input channels.  The transform renders all of the output channels,
// with each output channel rendered according to a weighted sum of the relevant
// input channels as defined in the matrix.
class ChannelMixer {
 public:
  // To mix two channels into one and preserve loudness, we must apply
  // (1 / sqrt(2)) gain to each.
  static constexpr float kHalfPower = 0.707106781186547524401f;

  ChannelMixer(ChannelLayout input_layout, ChannelLayout output_layout);
  ~ChannelMixer();

  // TODO(henrika): add comments....
  void Transform(AudioFrame* frame);

 private:
  void Initialize(ChannelLayout input_layout,
                  int input_channels,
                  ChannelLayout output_layout,
                  int output_channels);

  bool UpMixing() const { return output_channels_ > input_channels_; }
  bool DownMixing() const { return output_channels_ < input_channels_; }

  // Selected channel layouts.
  ChannelLayout input_layout_;
  ChannelLayout output_layout_;

  // Channel counts for input and output.
  int input_channels_;
  int output_channels_;

  // 2D matrix of output channels to input channels.
  std::vector<std::vector<float> > matrix_;

  // Optimization case for when we can simply remap the input channels to output
  // channels, i.e., when all scaling factors in |matrix_| equals 1.0.
  bool remapping_;

  // Delete the copy constructor and assignment operator.
  ChannelMixer(const ChannelMixer& other) = delete;
  ChannelMixer& operator=(const ChannelMixer& other) = delete;
};

}  // namespace webrtc

#endif  // AUDIO_UTILITY_CHANNEL_MIXER_H_
