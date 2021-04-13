
/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SOURCE_ENUMERATOR_H_
#define MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SOURCE_ENUMERATOR_H_

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/win/window_capture_utils.h"

namespace webrtc {

// WgcCapturerWin is initialized with an implementation of this interface,
// which it uses to find capturable sources of a particular type. This way,
// WgcCapturerWin can remain source-agnostic.
class SourceEnumerator {
 public:
  virtual ~SourceEnumerator() = default;

  virtual bool FindAllSources(DesktopCapturer::SourceList* sources) = 0;
};

class WindowEnumerator final : public SourceEnumerator {
 public:
  WindowEnumerator() = default;

  WindowEnumerator(const WindowEnumerator&) = delete;
  WindowEnumerator& operator=(const WindowEnumerator&) = delete;

  ~WindowEnumerator() override = default;

  bool FindAllSources(DesktopCapturer::SourceList* sources) override;

 private:
  WindowCaptureHelperWin window_capture_helper_;
};

class ScreenEnumerator final : public SourceEnumerator {
 public:
  ScreenEnumerator() = default;

  ScreenEnumerator(const ScreenEnumerator&) = delete;
  ScreenEnumerator& operator=(const ScreenEnumerator&) = delete;

  ~ScreenEnumerator() override = default;

  bool FindAllSources(DesktopCapturer::SourceList* sources) override;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SOURCE_ENUMERATOR_H_
