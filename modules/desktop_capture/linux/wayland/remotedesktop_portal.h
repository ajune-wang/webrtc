/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_REMOTEDESKTOP_PORTAL_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_REMOTEDESKTOP_PORTAL_H_

#include <gio/gio.h>

#include <memory>
#include <string>

#include "modules/desktop_capture/linux/wayland/constants.h"
#include "modules/desktop_capture/linux/wayland/screencast_portal.h"

namespace webrtc {

class RemoteDesktopPortal {
 public:
  explicit RemoteDesktopPortal(CaptureSourceType source_type,
                               ScreenCastPortal::PortalNotifier* notifier);
  ~RemoteDesktopPortal();

  void Start();
  uint32_t pipewire_stream_node_id();
  int pipewire_socket_fd();

 private:
  std::unique_ptr<ScreenCastPortal> screencast_portal_;
  ScreenCastPortal::PortalNotifier* notifier_;

  GDBusConnection* connection_ = nullptr;
  GDBusProxy* proxy_ = nullptr;
  GCancellable* cancellable_ = nullptr;
  std::string portal_handle_;
  std::string session_handle_;
  std::string start_handle_;
  std::string devices_handle_;
  guint session_request_signal_id_ = 0;
  guint start_request_signal_id_ = 0;
  guint session_closed_signal_id_ = 0;
  guint devices_request_signal_id_ = 0;
  CaptureSourceType capture_source_type_ = CaptureSourceType::kScreen;

  void PortalFailed(RequestResponse result);

  static void OnProxyRequested(GObject* object,
                               GAsyncResult* result,
                               gpointer user_data);
  static void OnScreenCastPortalProxyRequested(GObject* object,
                                               GAsyncResult* result,
                                               gpointer user_data);
  static void OnDevicesRequested(GDBusProxy* proxy,
                                 GAsyncResult* result,
                                 gpointer user_data);

  void SessionRequest();
  void SetSessionHandleForScreenCastPortal();
  static void OnSessionRequested(GDBusProxy* proxy,
                                 GAsyncResult* result,
                                 gpointer user_data);
  void SourcesRequest();
  void SelectDevicesRequest();
  static void OnDevicesRequestResponseSignal(GDBusConnection* connection,
                                             const gchar* sender_name,
                                             const gchar* object_path,
                                             const gchar* interface_name,
                                             const gchar* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);

  static void OnSessionRequestResponseSignal(GDBusConnection* connection,
                                             const char* sender_name,
                                             const char* object_path,
                                             const char* interface_name,
                                             const char* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);
  static void OnSessionClosedSignal(GDBusConnection* connection,
                                    const char* sender_name,
                                    const char* object_path,
                                    const char* interface_name,
                                    const char* signal_name,
                                    GVariant* parameters,
                                    gpointer user_data);
  static void OnSourcesRequestResponseSignal(GDBusConnection* connection,
                                             const char* sender_name,
                                             const char* object_path,
                                             const char* interface_name,
                                             const char* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);

  void StartRequest();
  static void OnStartRequested(GDBusProxy* proxy,
                               GAsyncResult* result,
                               gpointer user_data);
  static void OnStartRequestResponseSignal(GDBusConnection* connection,
                                           const char* sender_name,
                                           const char* object_path,
                                           const char* interface_name,
                                           const char* signal_name,
                                           GVariant* parameters,
                                           gpointer user_data);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_REMOTEDESKTOP_PORTAL_H_
