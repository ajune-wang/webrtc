/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_DOWNSAMPLE_H_
#define COMMON_AUDIO_RNN_VAD_DOWNSAMPLE_H_

#include "api/array_view.h"

namespace webrtc {
namespace rnn_vad {

// Downsampler from 48k to 24k with naive anti-aliasing filter.
void Decimate48k24k(rtc::ArrayView<float> dst, rtc::ArrayView<const float> src);

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_DOWNSAMPLE_H_
