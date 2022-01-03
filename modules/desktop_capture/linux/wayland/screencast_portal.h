/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
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
#define typeof __typeof__

#include <string>

#include "absl/types/optional.h"

namespace webrtc {

class ScreenCastPortal {
 public:
  // Values are set based on source type property in
  // xdg-desktop-portal/screencast
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  enum class CaptureSourceType : uint32_t {
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

  // Interface that must be implemented by the ScreenCastPortal consumers.
  enum class RequestResponse {
    // Success, the request is carried out.
    SUCCESS,

    // The user cancelled the interaction.
    USER_CANCELLED,

    // The user interaction was ended in some other way.
    ERROR,

    MAX_VALUE = ERROR
  };

  class PortalNotifier {
   public:
    virtual void OnScreenCastRequestResult(RequestResponse result,
                                           uint32_t stream_node_id,
                                           uint32_t fd) = 0;
    virtual void OnScreenCastSessionClosed();

   protected:
    virtual ~PortalNotifier() {}
  };

  explicit ScreenCastPortal(CaptureSourceType source_type);
  ~ScreenCastPortal();

  // Initialize ScreenCastPortal with series of DBus calls where we try to
  // obtain all the required information, like PipeWire file descriptor and
  // PipeWire stream node ID.
  //
  // The observer will return whether the communication with xdg-desktop-portal
  // was successful and only then you will be able to get all the required
  // information in order to continue working with PipeWire.
  void Start(PortalNotifier* notifier);

 private:
  PortalNotifier* notifier_ = nullptr;

  uint32_t pw_stream_node_id_ = 0;
  int32_t pw_fd_ = -1;

  CaptureSourceType capture_source_type_ =
      ScreenCastPortal::CaptureSourceType::kScreen;

  CursorMode cursor_mode_ = ScreenCastPortal::CursorMode::kEmbedded;

  GDBusConnection* connection_ = nullptr;
  GDBusProxy* proxy_ = nullptr;
  GCancellable* cancellable_ = nullptr;
  std::string portal_handle_;
  std::string session_handle_;
  std::string sources_handle_;
  std::string start_handle_;
  uint32_t session_request_signal_id_ = 0;
  uint32_t sources_request_signal_id_ = 0;
  uint32_t start_request_signal_id_ = 0;
  uint32_t session_closed_signal_id_ = 0;

  void PortalFailed(RequestResponse result);

  uint32_t SetupRequestResponseSignal(const char* object_path,
                                      GDBusSignalCallback callback);

  static void OnProxyRequested(GObject* object,
                               GAsyncResult* result,
                               gpointer user_data);

  static std::string PrepareSignalHandle(GDBusConnection* connection,
                                         const char* token);

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

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_PORTAL_H_
