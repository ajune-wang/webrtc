/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/x_window_property.h"

namespace webrtc {

XWindowPropertyBase::XWindowPropertyBase() {}

XWindowPropertyBase::~XWindowPropertyBase() {
  if (data_)
    XFree(data_);
}

bool XWindowPropertyBase::is_valid() const {
  return is_valid_;
}

size_t XWindowPropertyBase::size() const {
  return size_;
}

}  // namespace webrtc
