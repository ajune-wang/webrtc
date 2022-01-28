/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_PORTAL_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_PORTAL_H_

#include <gio/gio.h>

#include <string>

#include "modules/desktop_capture/linux/wayland/constants.h"

namespace webrtc {

class ScreenCastPortal {
 public:
  using ProxyRequestResponseHandler = std::function<
      void(GObject* object, GAsyncResult* result, gpointer user_data)>;

  using SourcesRequestResponseSignalHandler =
      std::function<void(GDBusConnection* connection,
                         const char* sender_name,
                         const char* object_path,
                         const char* interface_name,
                         const char* signal_name,
                         GVariant* parameters,
                         gpointer user_data)>;

  // Interface that must be implemented by the ScreenCastPortal consumers.
  class PortalNotifier {
   public:
    virtual void OnScreenCastRequestResult(RequestResponse result,
                                           uint32_t stream_node_id,
                                           int fd) = 0;
    virtual void OnScreenCastSessionClosed() = 0;

   protected:
    PortalNotifier() = default;
    virtual ~PortalNotifier() = default;
  };

  explicit ScreenCastPortal(CaptureSourceType source_type,
                            PortalNotifier* notifier);
  explicit ScreenCastPortal(
      CaptureSourceType source_type,
      PortalNotifier* notifier,
      ProxyRequestResponseHandler proxy_request_response_handler,
      SourcesRequestResponseSignalHandler
          sources_request_response_signal_handler);
  ~ScreenCastPortal();

  // Initialize ScreenCastPortal with series of DBus calls where we try to
  // obtain all the required information, like PipeWire file descriptor and
  // PipeWire stream node ID.
  //
  // The observer will return whether the communication with xdg-desktop-portal
  // was successful and only then you will be able to get all the required
  // information in order to continue working with PipeWire.
  void Start();

  void SetSessionHandle(std::string session_handle);

  void SetProxyConnection(GDBusProxy* proxy);

  void SetPipewireStreamNodeId(uint32_t pw_stream_node_id);

  uint32_t pipewire_stream_node_id();

  int pipewire_socket_fd();

  void OpenPipeWireRemote();

  void PortalFailed(RequestResponse result);

  void SourcesRequest();

 private:
  PortalNotifier* notifier_;

  // A PipeWire stream ID of stream we will be connecting to
  uint32_t pw_stream_node_id_ = 0;
  // A file descriptor of PipeWire socket
  int pw_fd_ = -1;

  CaptureSourceType capture_source_type_ = CaptureSourceType::kScreen;
  ProxyRequestResponseHandler proxy_request_response_handler_;
  SourcesRequestResponseSignalHandler sources_request_response_signal_handler_;

  CursorMode cursor_mode_ = CursorMode::kEmbedded;

  GDBusConnection* connection_ = nullptr;
  GDBusProxy* proxy_ = nullptr;
  GCancellable* cancellable_ = nullptr;
  std::string portal_handle_;
  std::string session_handle_;
  std::string sources_handle_;
  std::string start_handle_;
  guint session_request_signal_id_ = 0;
  guint sources_request_signal_id_ = 0;
  guint start_request_signal_id_ = 0;
  guint session_closed_signal_id_ = 0;

  static void OnProxyRequested(GObject* object,
                               GAsyncResult* result,
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
  static void OnSessionClosedSignal(GDBusConnection* connection,
                                    const char* sender_name,
                                    const char* object_path,
                                    const char* interface_name,
                                    const char* signal_name,
                                    GVariant* parameters,
                                    gpointer user_data);
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

  static void OnOpenPipeWireRemoteRequested(GDBusProxy* proxy,
                                            GAsyncResult* result,
                                            gpointer user_data);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_PORTAL_H_
