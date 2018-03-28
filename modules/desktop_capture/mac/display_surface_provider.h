/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_PROVIDER_H_
#define MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_PROVIDER_H_

#include "modules/desktop_capture/mac/display_surface.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

class DisplaySurfaceProvider {
 public:
  DisplaySurfaceProvider() = default;
  virtual ~DisplaySurfaceProvider() = default;

  virtual rtc::scoped_refptr<DisplaySurface> GetSurfaceForDisplay(
      CGDirectDisplayID display_id) = 0;
  virtual void SetSurfaceForDisplay(
      CGDirectDisplayID display_id,
      rtc::scoped_refptr<DisplaySurface> surface) = 0;

  RTC_DISALLOW_COPY_AND_ASSIGN(DisplaySurfaceProvider);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_PROVIDER_H_
