/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_CGIMAGE_H_
#define MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_CGIMAGE_H_

#include <CoreGraphics/CoreGraphics.h>

#include "modules/desktop_capture/mac/display_surface.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "sdk/objc/Framework/Classes/Common/scoped_cftyperef.h"

namespace webrtc {

class DisplaySurfaceCGImage final : public DisplaySurface {
 public:
  // Create an image containing a snapshot of the display.
  static rtc::scoped_refptr<DisplaySurfaceCGImage>
  CreateFromCurrentDisplayState(CGDirectDisplayID display_id);

  // DisplaySurface interface.
  inline int width() override { return width_; }
  inline int height() override { return height_; }
  inline int bytes_per_pixel() override { return bytes_per_pixel_; }
  inline int bytes_per_row() override { return bytes_per_row_; }
  inline const uint8_t* data() override { return data_; }

 private:
  explicit DisplaySurfaceCGImage(rtc::ScopedCFTypeRef<CGImageRef> cg_image);
  ~DisplaySurfaceCGImage() override;

  rtc::ScopedCFTypeRef<CGImageRef> cg_image_;
  rtc::ScopedCFTypeRef<CFDataRef> cg_data_;

  int width_;
  int height_;
  int bytes_per_pixel_;
  int bytes_per_row_;
  const uint8_t* data_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DisplaySurfaceCGImage);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_DISPLAY_SURFACE_CGIMAGE_H_
