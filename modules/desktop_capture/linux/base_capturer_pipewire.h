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

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/utils/result.h>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/linux/egl_dmabuf.h"
#include "modules/desktop_capture/linux/xdg_desktop_portal.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

class BaseCapturerPipeWire : public DesktopCapturer {
 public:
  explicit BaseCapturerPipeWire();
  ~BaseCapturerPipeWire() override;

  static std::unique_ptr<DesktopCapturer> CreateRawCapturer(
      const DesktopCaptureOptions& options);

  // DesktopCapturer interface.
  void Start(Callback* delegate) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  // PipeWire types -->
  struct pw_context* pw_context_ = nullptr;
  struct pw_core* pw_core_ = nullptr;
  struct pw_stream* pw_stream_ = nullptr;
  struct pw_thread_loop* pw_main_loop_ = nullptr;

  spa_hook spa_core_listener_;
  spa_hook spa_stream_listener_;

  // event handlers
  pw_core_events pw_core_events_ = {};
  pw_stream_events pw_stream_events_ = {};

  struct spa_video_info_raw spa_video_format_;

  bool capturer_failed_ = false;
  int64_t modifier_;
  DesktopSize video_size_;
  DesktopSize desktop_size_ = {};
  DesktopCaptureOptions options_ = {};

  webrtc::Mutex current_frame_lock_;
  std::unique_ptr<BasicDesktopFrame> current_frame_;
  Callback* callback_ = nullptr;

  std::unique_ptr<EglDmaBuf> egl_dmabuf_;
  std::unique_ptr<XdgDesktopPortal> xdg_desktop_portal_;

  void Init();
  void InitPortal();
  void InitPipeWireTypes();
  void OnPortalResponse(bool result);

  pw_stream* CreateReceivingStream();
  void HandleBuffer(pw_buffer* buffer);

  void ConvertRGBxToBGRx(uint8_t* frame, uint32_t size);

  static void OnCoreError(void* data,
                          uint32_t id,
                          int seq,
                          int res,
                          const char* message);
  static void OnStreamParamChanged(void* data,
                                   uint32_t id,
                                   const struct spa_pod* format);
  static void OnStreamStateChanged(void* data,
                                   pw_stream_state old_state,
                                   pw_stream_state state,
                                   const char* error_message);

  static void OnStreamProcess(void* data);
  static void OnNewBuffer(void* data, uint32_t id);

  RTC_DISALLOW_COPY_AND_ASSIGN(BaseCapturerPipeWire);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_BASE_CAPTURER_PIPEWIRE_H_
