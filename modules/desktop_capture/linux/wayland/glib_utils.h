/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_GLIB_UTILS_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_GLIB_UTILS_H_

#include <gio/gio.h>

#include <string>

namespace webrtc {

// Returns a string path for signal handle based on the provided connection and
// token.
std::string PrepareSignalHandle(GDBusConnection* connection, const char* token);

// Sets up the callback to execute when a response signal is received for the
// given object.
uint32_t SetupRequestResponseSignal(GDBusConnection* connection,
                                    const char* object_path,
                                    GDBusSignalCallback callback,
                                    gpointer user_data);
}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_GLIB_UTILS_H_
