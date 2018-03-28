/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_PROVIDER_CGIMAGE_H_
#define MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_PROVIDER_CGIMAGE_H_

#include "modules/desktop_capture/mac/display_surface_cgimage.h"
#include "modules/desktop_capture/mac/display_surface_provider.h"

namespace webrtc {

class DisplaySurfaceProviderCGImage final : public DisplaySurfaceProvider {
 public:
  DisplaySurfaceProviderCGImage();
  ~DisplaySurfaceProviderCGImage() override;

  // DisplaySurfaceProvider interface.
  rtc::scoped_refptr<DisplaySurface> GetSurfaceForDisplay(
      CGDirectDisplayID display_id) override;
  void SetSurfaceForDisplay(
      CGDirectDisplayID display_id,
      rtc::scoped_refptr<DisplaySurface> surface) override;

  RTC_DISALLOW_COPY_AND_ASSIGN(DisplaySurfaceProviderCGImage);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_PROVIDER_CGIMAGE_H_
