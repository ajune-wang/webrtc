/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/selected_window_context.h"

namespace webrtc {

SelectedWindowContext::SelectedWindowContext(
    HWND selected_window,
    DesktopRect selected_window_rect,
    WindowCaptureHelperWin* window_capture_helper)
    : selected_window_(selected_window),
      selected_window_rect_(selected_window_rect),
      window_capture_helper_(window_capture_helper) {
  selected_window_thread_id_ =
      GetWindowThreadProcessId(selected_window, &selected_window_process_id_);
}

bool SelectedWindowContext::IsSelectedWindowValid() const {
  return selected_window_thread_id_ != 0;
}

bool SelectedWindowContext::IsWindowOwnedBySelectedWindow(HWND hwnd) const {
  // This check works for drop-down menus & dialog pop-up windows.
  if (GetAncestor(hwnd, GA_ROOTOWNER) == selected_window_) {
    return true;
  }

  // Current implementation can't detect ownwership for some context menus or
  // tooltips. The approach with checking that both windows share the same
  // process and thread had too many false positive results and could lead to
  // having unintended windows in captured stream (see
  // https://bugs.chromium.org/p/webrtc/issues/detail?id=11455). Here is an
  // attempt to err on the side of caution returning false when we can't be
  // sure if |hwnd| is owned by selected window.
  return false;
}

bool SelectedWindowContext::IsWindowOverlappingSelectedWindow(HWND hwnd) const {
  return window_capture_helper_->AreWindowsOverlapping(hwnd, selected_window_,
                                                       selected_window_rect_);
}

HWND SelectedWindowContext::selected_window() const {
  return selected_window_;
}

WindowCaptureHelperWin* SelectedWindowContext::window_capture_helper() const {
  return window_capture_helper_;
}

}  // namespace webrtc
