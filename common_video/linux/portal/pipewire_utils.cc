/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/linux/portal/pipewire_utils.h"

#include <pipewire/pipewire.h>

#include "rtc_base/logging.h"

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
#include "common_video/linux/portal/pipewire_stubs.h"
using common_video_linux_portal::InitializeStubs;
using common_video_linux_portal::kModuleDrm;
using common_video_linux_portal::kModulePipewire;
using common_video_linux_portal::StubPathMap;
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

namespace webrtc {

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
const char kPipeWireLib[] = "libpipewire-0.3.so.0";
const char kDrmLib[] = "libdrm.so.2";
#endif

bool InitializePipewire() {
#if defined(WEBRTC_DLOPEN_PIPEWIRE)
  StubPathMap paths;

  // Check if the PipeWire and DRM libraries are available.
  paths[kModulePipewire].push_back(kPipeWireLib);
  paths[kModuleDrm].push_back(kDrmLib);

  if (!InitializeStubs(paths)) {
    RTC_LOG(LS_ERROR)
        << "One of following libraries is missing on your system:\n"
        << " - PipeWire (" << kPipeWireLib << ")\n"
        << " - drm (" << kDrmLib << ")";
    return false;
  }
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)
  return true;
}

PipeWireThreadLoopLock::PipeWireThreadLoopLock(pw_thread_loop* loop)
    : loop_(loop) {
  pw_thread_loop_lock(loop_);
}

PipeWireThreadLoopLock::~PipeWireThreadLoopLock() {
  pw_thread_loop_unlock(loop_);
}

}  // namespace webrtc
