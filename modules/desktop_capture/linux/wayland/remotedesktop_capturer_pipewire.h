/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_REMOTEDESKTOP_CAPTURER_PIPEWIRE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_REMOTEDESKTOP_CAPTURER_PIPEWIRE_H_

#include <memory>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/linux/wayland/base_capturer_pipewire.h"
#include "modules/desktop_capture/linux/wayland/remotedesktop_portal.h"

namespace webrtc {

class RemoteDesktopCapturer : public BaseCapturerPipeWire {
 public:
  explicit RemoteDesktopCapturer(const DesktopCaptureOptions& options);
  ~RemoteDesktopCapturer();

  RemoteDesktopCapturer(const RemoteDesktopCapturer&) = delete;
  RemoteDesktopCapturer& operator=(const RemoteDesktopCapturer&) = delete;

  // DesktopCapturer interface.
  void Start(Callback* delegate) override;

  // Populates session related details in the metadata so that input injection
  // module can make use of the same remote desktop session to inject inputs
  // on the remote host. Valid metadata can only be populated after the
  // capturer has been started using call to `Start()`.
  void PopulateMetadata(void* metadata) override;

 private:
  std::unique_ptr<RemoteDesktopPortal> remotedesktop_portal_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_REMOTEDESKTOP_CAPTURER_PIPEWIRE_H_
