/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/display_surface.h"

namespace webrtc {

DisplaySurface::DisplaySurface(int width,
                               int height,
                               int bytes_per_pixel,
                               int bytes_per_row)
    : width_(width),
      height_(height),
      bytes_per_pixel_(bytes_per_pixel),
      bytes_per_row_(bytes_per_row),
      data_(nullptr) {}

DisplaySurface::~DisplaySurface() = default;

}  // namespace webrtc
