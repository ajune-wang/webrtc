
/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/wgc_capture_source_enumerator.h"

#include "modules/desktop_capture/win/screen_capture_utils.h"

namespace webrtc {

bool WindowEnumerator::FindAllSources(DesktopCapturer::SourceList* sources) {
  // WGC fails to capture windows with the WS_EX_TOOLWINDOW style, so we provide
  // it as a filter to ensure windows with the style are not returned.
  return window_capture_helper_.EnumerateCapturableWindows(sources,
                                                           WS_EX_TOOLWINDOW);
}

bool ScreenEnumerator::FindAllSources(DesktopCapturer::SourceList* sources) {
  return webrtc::GetScreenList(sources);
}

}  // namespace webrtc
