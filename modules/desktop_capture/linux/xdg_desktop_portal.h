/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_XDG_DESKTOP_PORTAL_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_XDG_DESKTOP_PORTAL_H_

#include <gio/gio.h>
#define typeof __typeof__

#include "absl/types/optional.h"

namespace webrtc {

typedef std::function<void(bool)> PortalResponseCallback;

class XdgDesktopPortal {
 public:
  // Values are set based on source type property in
  // xdg-desktop-portal/screencast
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  enum class CaptureSourceType : uint32_t {
    kCamera = 0b00,
    kScreen = 0b01,
    kWindow = 0b10,
    kAnyScreenContent = kScreen | kWindow
  };

  // Values are set based on cursor mode property in
  // xdg-desktop-portal/screencast
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  enum class CursorMode : uint32_t {
    kHidden = 0b01,
    kEmbedded = 0b10,
    kMetadata = 0b100
  };

  explicit XdgDesktopPortal(CaptureSourceType source_type);
  ~XdgDesktopPortal();

  // Initialize XdgDesktopPortal with series of DBus calls where we try to
  // obtain all the required information, like PipeWire file descriptor and
  // PipeWire stream node ID.
  //
  // Camera portal: only PipeWire FD will be returned.
  // ScreenCast portal: both PipeWire FD and PipeWire stream ID will be
  // returned.
  //
  // The callback will return whether the communication with xdg-desktop-portal
  // was successful and only then you will be able to get all the required
  // information in order to continue working with PipeWire.
  void InitPortal(PortalResponseCallback callback);

  // Set cursor mode
  // Only used for ScreenCast portal.
  void SetCursorMode(CursorMode mode);

  absl::optional<uint32_t> PipeWireStreamNodeID() const;
  absl::optional<uint32_t> PipeWireFileDescriptor() const;

 private:
  PortalResponseCallback callback_;

  absl::optional<uint32_t> pw_stream_node_id_;
  absl::optional<int32_t> pw_fd_;

  CaptureSourceType capture_source_type_ =
      XdgDesktopPortal::CaptureSourceType::kScreen;

  // Request mouse cursor to be embedded as part of the stream, otherwise it
  // is hidden by default.
  CursorMode cursor_mode_ = XdgDesktopPortal::CursorMode::kEmbedded;

  GDBusConnection* connection_ = nullptr;
  GDBusProxy* proxy_ = nullptr;
  GCancellable* cancellable_ = nullptr;
  char* portal_handle_ = nullptr;
  char* access_camera_handle_ = nullptr;
  char* session_handle_ = nullptr;
  char* sources_handle_ = nullptr;
  char* start_handle_ = nullptr;
  uint32_t access_camera_request_signal_id_ = 0;
  uint32_t session_request_signal_id_ = 0;
  uint32_t sources_request_signal_id_ = 0;
  uint32_t start_request_signal_id_ = 0;

  uint32_t SetupRequestResponseSignal(const char* object_path,
                                      GDBusSignalCallback callback);

  static void OnProxyRequested(GObject* object,
                               GAsyncResult* result,
                               gpointer user_data);

  static char* PrepareSignalHandle(GDBusConnection* connection,
                                   const char* token);
  void AccessCamera();
  static void OnAccessCamera(GDBusProxy* proxy,
                             GAsyncResult* result,
                             gpointer user_data);
  static void OnAccessCameraResponseSignal(GDBusConnection* connection,
                                           const char* sender_name,
                                           const char* object_path,
                                           const char* interface_name,
                                           const char* signal_name,
                                           GVariant* parameters,
                                           gpointer user_data);

  void SessionRequest();
  static void OnSessionRequested(GDBusProxy* proxy,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnSessionRequestResponseSignal(GDBusConnection* connection,
                                             const char* sender_name,
                                             const char* object_path,
                                             const char* interface_name,
                                             const char* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);

  void SourcesRequest();
  static void OnSourcesRequested(GDBusProxy* proxy,
                                 GAsyncResult* result,
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

  void OpenPipeWireRemote();
  static void OnOpenPipeWireRemoteRequested(GDBusProxy* proxy,
                                            GAsyncResult* result,
                                            gpointer user_data);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_XDG_DESKTOP_PORTAL_H_
