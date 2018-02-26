/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/pitch_search.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// TODO(alessiob): Remove anon NS if empty.

}  // namespace

std::array<size_t, 2> CoarsePitchSearch(rtc::ArrayView<float> x) {
  // Decimate 2x (with no anti-aliasing filter).
  std::array<float, kHalfBufSize> x_decimated;
  for (size_t i = 0; i < x_decimated.size(); ++i)
    x_decimated[i] = x[2 * i];
  // TODO(alessiob): Implement.
  return {0, 0};
}

}  // namespace rnn_vad
}  // namespace webrtc
