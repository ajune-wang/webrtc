/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  // Use first bit to select if we parse just QP or the full header.
  if (size < 1) {
    return;
  }

  if (data[0] % 2 == 0) {
    int qp;
    vp9::GetQp(&data[1], size - 1, &qp);
  } else {
    vp9::ParseIntraFrameInfo(&data[1], size - 1);
  }
}
}  // namespace webrtc
