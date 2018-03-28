/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_H_
#define MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_H_

#include "api/refcountedbase.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class DisplaySurface : public rtc::RefCountedBase {
 public:
  DisplaySurface() = default;

  virtual int width() = 0;
  virtual int height() = 0;
  virtual int bytes_per_pixel() = 0;
  virtual int bytes_per_row() = 0;
  virtual const uint8_t* data() = 0;

 protected:
  ~DisplaySurface() override = default;

  RTC_DISALLOW_COPY_AND_ASSIGN(DisplaySurface);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_H_
