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

// Downsample from 48k to 24k.
void Decimate48k24k(rtc::ArrayView<const float> src, rtc::ArrayView<float> dst);

// 2x decimation without any anti-aliasing filter.
void Decimate2xNoAntiAliasignFilter(rtc::ArrayView<const float> src,
                                    rtc::ArrayView<float> dst);

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_DOWNSAMPLE_H_
