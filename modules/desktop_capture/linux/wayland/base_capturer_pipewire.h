/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_BASE_CAPTURER_PIPEWIRE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_BASE_CAPTURER_PIPEWIRE_H_

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/linux/wayland/portal_request_response.h"
#include "modules/desktop_capture/linux/wayland/screen_capture_portal_interface.h"
#include "modules/desktop_capture/linux/wayland/screencast_portal.h"
#include "modules/desktop_capture/linux/wayland/shared_screencast_stream.h"
#include "modules/desktop_capture/linux/wayland/xdg_desktop_portal_utils.h"
#include "modules/desktop_capture/linux/wayland/xdg_session_details.h"

namespace webrtc {

class BaseCapturerPipeWire : public DesktopCapturer,
                             public ScreenCastPortal::PortalNotifier {
 public:
  explicit BaseCapturerPipeWire(const DesktopCaptureOptions& options);
  BaseCapturerPipeWire(
      const DesktopCaptureOptions& options,
      std::unique_ptr<xdg_portal::ScreenCapturePortalInterface> portal);
  ~BaseCapturerPipeWire() override;

  BaseCapturerPipeWire(const BaseCapturerPipeWire&) = delete;
  BaseCapturerPipeWire& operator=(const BaseCapturerPipeWire&) = delete;

  // DesktopCapturer interface.
  void Start(Callback* delegate) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

  // ScreenCastPortal::PortalNotifier interface.
  void OnScreenCastRequestResult(xdg_portal::RequestResponse result,
                                 const SourceStreamIds& stream_node_ids,
                                 int fd) override;
  void OnScreenCastSessionClosed() override;
  void UpdateResolution(uint32_t width, uint32_t height) override;

  xdg_portal::SessionDetails GetSessionDetails();

 private:
  DesktopCaptureOptions options_ = {};
  Callback* callback_ = nullptr;
  bool capturer_failed_ = false;
  SourceId current_source_id_ = -1;
  SourceStreamIds stream_node_ids_;
  std::map<SourceId, /*pipewire_node_id=*/uint32_t> source_to_node_id_;
  // A file descriptor of PipeWire socket
  int pw_fd_ = -1;
  std::unique_ptr<xdg_portal::ScreenCapturePortalInterface> portal_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_BASE_CAPTURER_PIPEWIRE_H_
