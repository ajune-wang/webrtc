/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_XDG_SESSION_DETAILS_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_XDG_SESSION_DETAILS_H_

#include <gio/gio.h>

#include <string>
#include <vector>

#include "modules/desktop_capture/desktop_capture_types.h"

namespace webrtc {
namespace xdg_portal {

// Details of the session associated with XDG desktop portal session. Portal API
// calls can be invoked by utilizing the information here.
struct SessionDetails {
  GDBusProxy* proxy = nullptr;
  GCancellable* cancellable = nullptr;
  std::string session_handle;
  // Mapping from source id -> stream id.
  SourceStreamInfo source_stream_infos;
  PipeWireStreamInfo active_stream = {};
};

}  // namespace xdg_portal
}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_XDG_SESSION_DETAILS_H_
