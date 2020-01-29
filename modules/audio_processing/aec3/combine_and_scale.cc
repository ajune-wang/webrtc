/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/combine_and_scale.h"

#include <algorithm>
#include <functional>
#include <iterator>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {}  // namespace

CombineAndScale::CombineAndScale(size_t num_input_channels,
                                 size_t num_output_channels,
                                 float algorithmic_delay,
                                 bool modifies_input_signal)
    : num_input_channels_(num_input_channels),
      num_output_channels_(num_output_channels),
      algorithmic_delay_(algorithmic_delay),
      modifies_input_signal_(modifies_input_signal) {}

void CombineAndScale::Process(
    const std::vector<std::array<float, 65>*>& X0_fft_re,
    const std::vector<std::array<float, 65>*>& X0_fft_im,
    std::vector<std::vector<std::vector<float>>>* x) {
  static_cast<void>(num_input_channels_);
}

}  // namespace webrtc
