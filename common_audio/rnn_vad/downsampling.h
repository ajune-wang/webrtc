/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_DOWNSAMPLING_H_
#define COMMON_AUDIO_RNN_VAD_DOWNSAMPLING_H_

#include "api/array_view.h"
#include "common_audio/rnn_vad/features_extraction.h"

namespace webrtc {
namespace rnn_vad {

// 2x decimator with naive anti-aliasing filter used to downsample frames with
// a sample rate as twice as |kInputFrameSize|.
void Decimate2x(rtc::ArrayView<float, kInputFrameSize> dst,
                rtc::ArrayView<const float, 2 * kInputFrameSize> src) {
  dst[0] = 0.5 * src[0] + 0.25 * src[1];
  for (size_t i = 1; i < rnn_vad::kInputFrameSize; ++i)
    dst[i] = 0.25 * src[2 * i - 1] + 0.5 * src[2 * i] + 0.25 * src[2 * i + 1];
}

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_DOWNSAMPLING_H_
