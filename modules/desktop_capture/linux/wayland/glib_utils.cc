/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/glib_utils.h"

#include "modules/desktop_capture/linux/wayland/constants.h"
#include "modules/desktop_capture/linux/wayland/scoped_glib.h"

namespace webrtc {

std::string PrepareSignalHandle(GDBusConnection* connection,
                                const char* token) {
  Scoped<char> sender(
      g_strdup(g_dbus_connection_get_unique_name(connection) + 1));
  for (int i = 0; sender.get()[i]; ++i) {
    if (sender.get()[i] == '.') {
      sender.get()[i] = '_';
    }
  }
  const char* handle = g_strconcat(kDesktopRequestObjectPath, "/", sender.get(),
                                   "/", token, /*end of varargs*/ nullptr);
  return handle;
}

uint32_t SetupRequestResponseSignal(GDBusConnection* connection,
                                    const char* object_path,
                                    GDBusSignalCallback callback,
                                    gpointer user_data) {
  return g_dbus_connection_signal_subscribe(
      connection, kDesktopBusName, kRequestInterfaceName, "Response",
      object_path, /*arg0=*/nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
      callback, user_data, /*user_data_free_func=*/nullptr);
}

}  // namespace webrtc
