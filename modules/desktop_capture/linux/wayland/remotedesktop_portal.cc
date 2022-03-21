/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/remotedesktop_portal.h"

#include <glib-object.h>

#include <utility>

#include "modules/desktop_capture/linux/wayland/scoped_glib.h"
#include "modules/desktop_capture/linux/wayland/xdg_desktop_portal_utils.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

using xdg_portal::kDesktopObjectPath;
using xdg_portal::kRemoteDesktopInterfaceName;
using xdg_portal::kSessionInterfaceName;
using xdg_portal::PrepareSignalHandle;
using xdg_portal::RequestResponse;
using xdg_portal::RequestResponseToString;
using xdg_portal::RequestSessionProxy;
using xdg_portal::RequestSessionUsingProxy;
using xdg_portal::SessionRequestHandler;
using xdg_portal::SessionRequestResponseSignalHelper;
using xdg_portal::SetupRequestResponseSignal;
using xdg_portal::SetupSessionRequestHandlers;
using xdg_portal::StartRequestedHandler;
using xdg_portal::StartSessionRequest;
using xdg_portal::TearDownSession;

void UnsubscribeSignalHandler(GDBusConnection* connection, guint& signal_id) {
  if (signal_id) {
    g_dbus_connection_signal_unsubscribe(connection, signal_id);
    signal_id = 0;
  }
}

}  // namespace

RemoteDesktopPortal::RemoteDesktopPortal(
    ScreenCastPortal::PortalNotifier* notifier) {
  cancellable_ = g_cancellable_new();
  screencast_portal_ = std::make_unique<ScreenCastPortal>(
      ScreenCastPortal::CaptureSourceType::kAnyScreenContent, notifier,
      OnScreenCastPortalProxyRequested, OnSourcesRequestResponseSignal, this);
  screencast_portal_->SetSessionDetails({.cancellable = cancellable_});
}

RemoteDesktopPortal::~RemoteDesktopPortal() {
  UnsubscribeSignalHandlers();
  TearDownSession(std::move(session_handle_), proxy_, cancellable_,
                  connection_);
  cancellable_ = nullptr;
  proxy_ = nullptr;
}

void RemoteDesktopPortal::UnsubscribeSignalHandlers() {
  UnsubscribeSignalHandler(connection_, start_request_signal_id_);
  UnsubscribeSignalHandler(connection_, session_request_signal_id_);
  UnsubscribeSignalHandler(connection_, devices_request_signal_id_);
  UnsubscribeSignalHandler(connection_, session_closed_signal_id_);
}

void RemoteDesktopPortal::Start() {
  RTC_LOG(LS_INFO) << "Starting screen cast portal";
  screencast_portal_->Start();
  RTC_LOG(LS_INFO) << "Starting remote desktop portal";
  RequestSessionProxy(kRemoteDesktopInterfaceName, OnProxyRequested,
                      cancellable_, this);
}

uint32_t RemoteDesktopPortal::pipewire_stream_node_id() {
  return screencast_portal_->pipewire_stream_node_id();
}

void RemoteDesktopPortal::OnProxyRequested(GObject* gobject,
                                           GAsyncResult* result,
                                           gpointer user_data) {
  RequestSessionUsingProxy<RemoteDesktopPortal>(
      static_cast<RemoteDesktopPortal*>(user_data), gobject, result);
}

// static
void RemoteDesktopPortal::OnScreenCastPortalProxyRequested(GObject* /*object*/,
                                                           GAsyncResult* result,
                                                           gpointer user_data) {
  ScreenCastPortal* that = static_cast<ScreenCastPortal*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  GDBusProxy* proxy = g_dbus_proxy_new_finish(result, error.receive());
  if (!proxy) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to create a proxy for the screen cast portal: "
                      << error->message;
    that->PortalFailed(RequestResponse::kError);
    return;
  }
  that->SetSessionDetails({.proxy = proxy});

  RTC_LOG(LS_INFO) << "Successfully created proxy for the screen cast portal.";
}

void RemoteDesktopPortal::SessionRequest(GDBusProxy* proxy) {
  proxy_ = proxy;
  connection_ = g_dbus_proxy_get_connection(proxy_);

  SetupSessionRequestHandlers("remotedesktop", OnSessionRequested,
                              OnSessionRequestResponseSignal, connection_,
                              proxy_, cancellable_, portal_handle_,
                              session_request_signal_id_, this);
}

void RemoteDesktopPortal::OnSessionRequested(GDBusProxy* proxy,
                                             GAsyncResult* result,
                                             gpointer user_data) {
  SessionRequestHandler(static_cast<RemoteDesktopPortal*>(user_data), proxy,
                        result, user_data);
}

void RemoteDesktopPortal::OnDevicesRequested(GDBusProxy* proxy,
                                             GAsyncResult* result,
                                             gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    RTC_LOG(LS_ERROR) << "Failed to select the devices: " << error->message;
    return;
  }

  Scoped<gchar> handle;
  g_variant_get_child(variant.get(), 0, "o", handle.receive());
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the remote desktop session.";
    if (that->devices_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->devices_request_signal_id_);
      that->devices_request_signal_id_ = 0;
    }
    return;
  }
  RTC_LOG(LS_INFO) << "Subscribed to devices signal.";
}

void RemoteDesktopPortal::SourcesRequest() {
  screencast_portal_->SourcesRequest();
}

void RemoteDesktopPortal::OnDevicesRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  RTC_LOG(LS_INFO) << "Received device selection signal from session.";
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  RTC_DCHECK(that);

  guint32 portal_response;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, nullptr);
  if (portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to select devices for the remote desktop session.";
    return;
  }
  that->SourcesRequest();
}

void RemoteDesktopPortal::SelectDevicesRequest() {
  GVariantBuilder builder;
  Scoped<gchar> variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&builder, "{sv}", "multiple",
                        g_variant_new_boolean(false));

  variant_string =
      g_strdup_printf("remotedesktop%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  devices_handle_ = PrepareSignalHandle(variant_string.get(), connection_);
  devices_request_signal_id_ = SetupRequestResponseSignal(
      devices_handle_.c_str(), OnDevicesRequestResponseSignal, this,
      connection_);

  RTC_LOG(LS_ERROR) << "Selecting devices from the remote desktop session.";
  g_dbus_proxy_call(
      proxy_, "SelectDevices",
      g_variant_new("(oa{sv})", session_handle_.c_str(), &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnDevicesRequested), this);
}

void RemoteDesktopPortal::OnSessionRequestResponseSignal(
    GDBusConnection* connection,
    const char* sender_name,
    const char* object_path,
    const char* interface_name,
    const char* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  RTC_DCHECK(that);
  SessionRequestResponseSignalHelper(
      OnSessionClosedSignal, that, that->connection_, that->session_handle_,
      parameters, that->session_closed_signal_id_);
  that->screencast_portal_->SetSessionDetails(
    {.session_handle = that->session_handle_});
  that->SelectDevicesRequest();
}

void RemoteDesktopPortal::OnSessionClosedSignal(GDBusConnection* connection,
                                                const char* sender_name,
                                                const char* object_path,
                                                const char* interface_name,
                                                const char* signal_name,
                                                GVariant* parameters,
                                                gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "Received closed signal from session.";

  // Unsubscribe from the signal and free the session handle to avoid calling
  // Session::Close from the destructor since it's already closed
  g_dbus_connection_signal_unsubscribe(that->connection_,
                                       that->session_closed_signal_id_);
}

// static
void RemoteDesktopPortal::OnSourcesRequestResponseSignal(
    GDBusConnection* connection,
    const char* sender_name,
    const char* object_path,
    const char* interface_name,
    const char* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "Received sources signal from session.";

  uint32_t portal_response;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, nullptr);
  if (portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to select sources for the remote desktop session.";
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  that->StartRequest();
}

void RemoteDesktopPortal::StartRequest() {
  StartSessionRequest("remotedesktop", session_handle_,
                      OnStartRequestResponseSignal, OnStartRequested, proxy_,
                      connection_, cancellable_, start_request_signal_id_,
                      start_handle_, this);
}

void RemoteDesktopPortal::OnStartRequested(GDBusProxy* proxy,
                                           GAsyncResult* result,
                                           gpointer user_data) {
  StartRequestedHandler(static_cast<RemoteDesktopPortal*>(user_data), proxy,
                        result);
}

void RemoteDesktopPortal::OnStartRequestResponseSignal(
    GDBusConnection* connection,
    const char* sender_name,
    const char* object_path,
    const char* interface_name,
    const char* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "Start signal received.";
  uint32_t portal_response;
  Scoped<GVariant> response_data;
  Scoped<GVariantIter> iter;
  g_variant_get(parameters, "(u@a{sv})", &portal_response,
                response_data.receive());
  if (portal_response || !response_data) {
    RTC_LOG(LS_ERROR) << "Failed to start the remote desktop session.";
    return;
  }

  if (g_variant_lookup(response_data.get(), "streams", "a(ua{sv})",
                       iter.receive())) {
    Scoped<GVariant> variant;

    while (g_variant_iter_next(iter.get(), "@(ua{sv})", variant.receive())) {
      uint32_t stream_id;
      Scoped<GVariant> options;

      g_variant_get(variant.get(), "(u@a{sv})", &stream_id, options.receive());
      RTC_DCHECK(options.get());

      that->screencast_portal_->SetSessionDetails(
        {.pipewire_stream_node_id = stream_id});
      break;
    }
  }

  that->screencast_portal_->OpenPipeWireRemote();
  RTC_LOG(LS_INFO) << "Remote desktop portal start response successful";
}

void RemoteDesktopPortal::PortalFailed(RequestResponse result) {
  RTC_LOG(LS_ERROR) << "Remote desktop portal failure, reason: "
                    << RequestResponseToString(result);
}

}  // namespace webrtc
