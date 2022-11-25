/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>

#include <memory>

#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/mouse_cursor_monitor.h"
#include "rtc_base/logging.h"

namespace webrtc {

MouseCursorMonitor* MouseCursorMonitor::CreateForWindow(
    const DesktopCaptureOptions& options,
    WindowId window) {
  RTC_LOG(LS_INFO) << __func__;

  return NULL;
}

MouseCursorMonitor* MouseCursorMonitor::CreateForScreen(
    const DesktopCaptureOptions& options,
    ScreenId screen) {
  RTC_LOG(LS_INFO) << __func__;

  return NULL;
}

std::unique_ptr<MouseCursorMonitor> MouseCursorMonitor::Create(
    const DesktopCaptureOptions& options) {
  RTC_LOG(LS_INFO) << __func__;

  return std::unique_ptr<MouseCursorMonitor>(
      CreateForScreen(options, kFullDesktopScreenId));
}

}  // namespace webrtc
