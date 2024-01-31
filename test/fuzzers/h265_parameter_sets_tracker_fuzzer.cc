/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdint.h>

#include "modules/video_coding/h265_parameter_sets_tracker.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  video_coding::H265ParameterSetsTracker h265_parameter_sets_tracker;
  h265_parameter_sets_tracker.MaybeFixBitstream(
      rtc::ArrayView<const uint8_t>(data, size));
}
}  // namespace webrtc
