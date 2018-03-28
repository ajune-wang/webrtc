/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/display_surface_provider_cgimage.h"

namespace webrtc {

DisplaySurfaceProviderCGImage::DisplaySurfaceProviderCGImage() {}

DisplaySurfaceProviderCGImage::~DisplaySurfaceProviderCGImage() {}

rtc::scoped_refptr<DisplaySurface> DisplaySurfaceProviderCGImage::GetSurfaceForDisplay(
    CGDirectDisplayID display_id) {
  return DisplaySurfaceCGImage::CreateFromCurrentDisplayState(display_id);
}

void DisplaySurfaceProviderCGImage::SetSurfaceForDisplay(
    CGDirectDisplayID display_id, rtc::scoped_refptr<DisplaySurface> surface) {}

}  // namespace webrtc
