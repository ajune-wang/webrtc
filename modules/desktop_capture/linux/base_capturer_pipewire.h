/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_BASE_CAPTURER_PIPEWIRE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_BASE_CAPTURER_PIPEWIRE_H_

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/linux/xdg_desktop_portal_base.h"

#include "api/ref_counted_base.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

class RTC_EXPORT BaseCapturerPipeWire : public DesktopCapturer {
 public:
  explicit BaseCapturerPipeWire(const DesktopCaptureOptions& options,
                                XdgDesktopPortalBase::CaptureSourceType type);
  ~BaseCapturerPipeWire() override;

  // There are three scenarios when the Init() method might be called:
  // 1) The browser will set ID through DesktopCapturerOptions, in this case
  //    we can initialize it right away in the constructor as we have ID for
  //    the portal request.
  // 2) The browser will not set ID, those will be browsers not using new API
  //    to avoid additional portal calls. In this case the Init() will be called
  //    when the browser asks for sources. This is because there might be a
  //    case when the browser doesn't ask for sources, but immediately knows
  //    ID of the source he wants to show (ID picked by the preview dialog).
  // 3) The browser asks for specific source passing an ID using SelectSource()
  //    method. This will be in case user gets past the preview dialog and the
  //    browser wants to show the same source on the web page without further
  //    asking.
  bool Init();

  void Start(Callback* delegate) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

  static std::unique_ptr<DesktopCapturer> CreateRawScreenCapturer(
      const DesktopCaptureOptions& options);

  static std::unique_ptr<DesktopCapturer> CreateRawWindowCapturer(
      const DesktopCaptureOptions& options);

 private:
  Callback* callback_ = nullptr;

  XdgDesktopPortalBase::CaptureSourceType type_ =
      XdgDesktopPortalBase::CaptureSourceType::kScreen;
  int32_t id_;
  bool init_called_ = false;
  bool portal_initialized_ = false;
  bool auto_close_connection_ = false;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_BASE_CAPTURER_PIPEWIRE_H_
