/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/remotedesktop_capturer_pipewire.h"

#include <memory>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

RemoteDesktopCapturer::RemoteDesktopCapturer(
    const DesktopCaptureOptions& options)
    : BaseCapturerPipeWire(options) {
  remotedesktop_portal_ = std::make_unique<RemoteDesktopPortal>(
      CaptureSourceType::kAnyScreenContent, this);
}

RemoteDesktopCapturer::~RemoteDesktopCapturer() {}

void RemoteDesktopCapturer::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  BaseCapturerPipeWire::callback_ = callback;

  remotedesktop_portal_->Start();
}

void RemoteDesktopCapturer::PopulateMetadata(void* metadata) {
  remotedesktop_portal_->PopulateSessionDetails(metadata);
}

}  // namespace webrtc
