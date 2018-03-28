/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/display_surface_cgimage.h"

#include "rtc_base/checks.h"

namespace webrtc {

// static
rtc::scoped_refptr<DisplaySurfaceCGImage> DisplaySurfaceCGImage::CreateFromCurrentDisplayState(
    CGDirectDisplayID display_id) {
  rtc::ScopedCFTypeRef<CGImageRef> cg_image(CGDisplayCreateImage(display_id));
  if (cg_image) {
    return rtc::scoped_refptr<DisplaySurfaceCGImage>(new DisplaySurfaceCGImage(cg_image));
  }

  return nullptr;
}

DisplaySurfaceCGImage::DisplaySurfaceCGImage(rtc::ScopedCFTypeRef<CGImageRef> cg_image)
    : DisplaySurfaceCGImage(CGImageGetWidth(cg_image.get()),
                            CGImageGetHeight(cg_image.get()),
                            CGImageGetBitsPerPixel(cg_image.get()) / 8,
                            CGImageGetBytesPerRow(cg_image.get())),
      cg_image_(cg_image) {
  RTC_DCHECK(cg_image_);

  // Request access to the raw pixel data via the image's DataProvider.
  CGDataProviderRef cg_provider = CGImageGetDataProvider(cg_image.get());
  RTC_DCHECK(cg_provider);

  rtc::ScopedCFTypeRef<CFDataRef> cg_data(CGDataProviderCopyData(cg_provider));
  RTC_DCHECK(cg_data);

  data_ = CFDataGetBytePtr(cg_data.get());
  RTC_DCHECK(data_);
}

DisplaySurfaceCGImage::~DisplaySurfaceCGImage() {
  RTC_DCHECK(cg_image_);
}

}  // namespace webrtc
