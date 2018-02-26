/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_PITCH_SEARCH_H_
#define COMMON_AUDIO_RNN_VAD_PITCH_SEARCH_H_

#include <array>

#include "api/array_view.h"
#include "common_audio/rnn_vad/common.h"

namespace webrtc {
namespace rnn_vad {

// Search best and second best pitch candidates using a 2x decimated version
// of the input |x|.
std::array<size_t, 2> CoarsePitchSearch(rtc::ArrayView<float> x);

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_PITCH_SEARCH_H_
