/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_X_WINDOW_PROPERTY_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_X_WINDOW_PROPERTY_H_

#include <X11/X.h>
#include <X11/Xlib.h>

#include "rtc_base/constructor_magic.h"

namespace webrtc {

// Convenience wrapper for XGetWindowProperty() results.
template <class PropertyType>
class XWindowProperty {
 public:
  XWindowProperty(Display* display, Window window, Atom property) {
    const int kBitsPerByte = 8;
    Atom actual_type;
    int actual_format;
    unsigned long bytes_after;  // NOLINT: type required by XGetWindowProperty
    int status = XGetWindowProperty(
        display, window, property, 0L, ~0L, False, AnyPropertyType,
        &actual_type, &actual_format, &size_, &bytes_after, &data_);
    if (status != Success) {
      data_ = nullptr;
      return;
    }
    if (sizeof(PropertyType) * kBitsPerByte != actual_format) {
      size_ = 0;
      return;
    }

    is_valid_ = true;
  }

  ~XWindowProperty() {
    if (data_)
      XFree(data_);
  }

  // True if we got properly value successfully.
  bool is_valid() const { return is_valid_; }

  // Size and value of the property.
  size_t size() const { return size_; }
  const PropertyType* data() const {
    return reinterpret_cast<PropertyType*>(data_);
  }
  PropertyType* data() { return reinterpret_cast<PropertyType*>(data_); }

 private:
  bool is_valid_ = false;
  unsigned long size_ = 0;  // NOLINT: type required by XGetWindowProperty
  unsigned char* data_ = nullptr;

  RTC_DISALLOW_COPY_AND_ASSIGN(XWindowProperty);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_X_WINDOW_PROPERTY_H_
