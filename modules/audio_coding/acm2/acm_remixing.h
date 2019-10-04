/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_ACM2_ACM_REMIXING_H_
#define MODULES_AUDIO_CODING_ACM2_ACM_REMIXING_H_

#include <vector>

#include "api/audio/audio_frame.h"

namespace webrtc {

// Stereo-to-mono downmixing that can be used as in-place.
void DownMixFrame(const AudioFrame& frame,
                  size_t length_out_buff,
                  int16_t* out_buff);

// Remixes the input frame to an output data vector. The output vector is
// resized if needed.
void ReMixFrame(const AudioFrame& input,
                size_t num_output_channels,
                std::vector<int16_t>* output);

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_ACM2_ACM_REMIXING_H_
