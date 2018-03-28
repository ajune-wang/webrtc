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
  DisplaySurface(int width,
                 int height,
                 int bytes_per_pixel,
                 int bytes_per_row,
                 const uint8_t* data);

  int width() { return width_; }
  int height() { return height_; }
  int bytes_per_pixel() { return bytes_per_pixel_; }
  int bytes_per_row() { return bytes_per_row_; }
  const uint8_t* data() { return data_; }

 protected:
  ~DisplaySurface() override;

 private:
  int width_;
  int height_;
  int bytes_per_pixel_;
  int bytes_per_row_;
  const uint8_t* data_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DisplaySurface);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_H_
