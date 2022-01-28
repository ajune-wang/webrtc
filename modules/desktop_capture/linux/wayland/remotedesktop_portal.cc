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

#include "modules/desktop_capture/linux/wayland/constants.h"
#include "modules/desktop_capture/linux/wayland/glib_utils.h"
#include "modules/desktop_capture/linux/wayland/scoped_glib.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

RemoteDesktopPortal::RemoteDesktopPortal(
    CaptureSourceType source_type,
    ScreenCastPortal::PortalNotifier* notifier)
    : notifier_(notifier) {
  screencast_portal_ = std::make_unique<ScreenCastPortal>(
      CaptureSourceType::kAnyScreenContent, notifier,
      OnScreenCastPortalProxyRequested, OnSourcesRequestResponseSignal);
}

RemoteDesktopPortal::~RemoteDesktopPortal() {
  if (start_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_, start_request_signal_id_);
  }
  if (session_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         session_request_signal_id_);
  }

  if (!session_handle_.empty()) {
    Scoped<GDBusMessage> message(
        g_dbus_message_new_method_call(kDesktopBusName, session_handle_.c_str(),
                                       kSessionInterfaceName, "Close"));
    if (message.get()) {
      Scoped<GError> error;
      g_dbus_connection_send_message(connection_, message.get(),
                                     G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                     /*out_serial=*/nullptr, error.receive());
      if (error.get()) {
        RTC_LOG(LS_ERROR) << "Failed to close the session: " << error->message;
      }
    }
  }

  if (cancellable_) {
    g_cancellable_cancel(cancellable_);
    g_object_unref(cancellable_);
    cancellable_ = nullptr;
  }

  if (proxy_) {
    g_object_unref(proxy_);
    proxy_ = nullptr;
  }
}

void RemoteDesktopPortal::Start() {
  cancellable_ = g_cancellable_new();
  screencast_portal_->Start();
  g_dbus_proxy_new_for_bus(
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, /*info=*/nullptr,
      kDesktopBusName, kDesktopObjectPath, kRemoteDesktopInterfaceName,
      cancellable_, reinterpret_cast<GAsyncReadyCallback>(OnProxyRequested),
      this);
}

void RemoteDesktopPortal::PortalFailed(RequestResponse result) {
  notifier_->OnScreenCastRequestResult(result, pipewire_stream_node_id(),
                                       pipewire_socket_fd());
}

uint32_t RemoteDesktopPortal::pipewire_stream_node_id() {
  return screencast_portal_->pipewire_stream_node_id();
}

int RemoteDesktopPortal::pipewire_socket_fd() {
  return screencast_portal_->pipewire_socket_fd();
}

// static
void RemoteDesktopPortal::OnProxyRequested(GObject* /*object*/,
                                           GAsyncResult* result,
                                           gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  GDBusProxy* proxy = g_dbus_proxy_new_finish(result, error.receive());
  if (!proxy) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to create a proxy for the remote desktop "
                      << "portal: " << error->message;
    that->PortalFailed(RequestResponse::kError);
    return;
  }
  that->proxy_ = proxy;
  that->connection_ = g_dbus_proxy_get_connection(that->proxy_);

  RTC_LOG(LS_INFO) << "Created proxy for the remote desktop portal.";

  that->SessionRequest();
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
  that->SetProxyConnection(proxy);

  RTC_LOG(LS_INFO) << "Successfully created proxy for the screen cast portal.";
}

void RemoteDesktopPortal::SessionRequest() {
  GVariantBuilder builder;
  Scoped<char> variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string =
      g_strdup_printf("webrtc_session%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "session_handle_token",
                        g_variant_new_string(variant_string.get()));
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  portal_handle_ = PrepareSignalHandle(connection_, variant_string.get());
  session_request_signal_id_ =
      SetupRequestResponseSignal(connection_, portal_handle_.c_str(),
                                 OnSessionRequestResponseSignal, this);

  RTC_LOG(LS_INFO) << "Remote desktop session requested.";
  g_dbus_proxy_call(proxy_, "CreateSession", g_variant_new("(a{sv})", &builder),
                    G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
                    reinterpret_cast<GAsyncReadyCallback>(OnSessionRequested),
                    this);
}

// static
void RemoteDesktopPortal::OnSessionRequested(GDBusProxy* proxy,
                                             GAsyncResult* result,
                                             gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to create a remote desktop session: "
                      << error->message;
    that->PortalFailed(RequestResponse::kError);
    return;
  }
  RTC_LOG(LS_INFO) << "Initializing the remote desktop session.";

  Scoped<char> handle;
  g_variant_get_child(variant.get(), 0, "o", &handle);
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the remote desktop session.";
    if (that->session_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->session_request_signal_id_);
      that->session_request_signal_id_ = 0;
    }
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  RTC_LOG(LS_INFO) << "Subscribing to the remote desktop session.";
}

// static
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
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  Scoped<gchar> handle;
  g_variant_get_child(variant.get(), 0, "o", handle.receive());
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the remote desktop session.";
    that->PortalFailed(RequestResponse::kError);
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

// static
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
    that->PortalFailed(RequestResponse::kError);
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

  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  devices_handle_ = PrepareSignalHandle(connection_, variant_string.get());
  devices_request_signal_id_ =
      SetupRequestResponseSignal(connection_, devices_handle_.c_str(),
                                 OnDevicesRequestResponseSignal, this);

  RTC_LOG(LS_ERROR) << "Selecting devices from the remote desktop session.";
  g_dbus_proxy_call(
      proxy_, "SelectDevices",
      g_variant_new("(oa{sv})", session_handle_.c_str(), &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnDevicesRequested), this);
}

void RemoteDesktopPortal::SetSessionHandleForScreenCastPortal() {
  screencast_portal_->SetSessionHandle(session_handle_);
}

// static
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

  RTC_LOG(LS_INFO)
      << "Received response for the remote desktop session subscription.";

  uint32_t portal_response;
  Scoped<GVariant> response_data;
  g_variant_get(parameters, "(u@a{sv})", &portal_response,
                response_data.receive());
  Scoped<GVariant> session_handle(
      g_variant_lookup_value(response_data.get(), "session_handle", nullptr));
  that->session_handle_ = g_variant_dup_string(session_handle.get(), nullptr);

  if (that->session_handle_.empty() || portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to request the remote desktop session subscription.";
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  that->SetSessionHandleForScreenCastPortal();

  that->session_closed_signal_id_ = g_dbus_connection_signal_subscribe(
      that->connection_, kDesktopBusName, kSessionInterfaceName, "Closed",
      that->session_handle_.c_str(), /*arg0=*/nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
      OnSessionClosedSignal, that, /*user_data_free_func=*/nullptr);

  that->SelectDevicesRequest();
}

// static
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

  that->notifier_->OnScreenCastSessionClosed();

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
  GVariantBuilder builder;
  Scoped<char> variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  start_handle_ = PrepareSignalHandle(connection_, variant_string.get());
  start_request_signal_id_ = SetupRequestResponseSignal(
      connection_, start_handle_.c_str(), OnStartRequestResponseSignal, this);

  // "Identifier for the application window", this is Wayland, so not "x11:...".
  const char parent_window[] = "";

  RTC_LOG(LS_INFO) << "Starting the remote desktop session.";
  g_dbus_proxy_call(proxy_, "Start",
                    g_variant_new("(osa{sv})", session_handle_.c_str(),
                                  parent_window, &builder),
                    G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
                    reinterpret_cast<GAsyncReadyCallback>(OnStartRequested),
                    this);
}

// static
void RemoteDesktopPortal::OnStartRequested(GDBusProxy* proxy,
                                           GAsyncResult* result,
                                           gpointer user_data) {
  RemoteDesktopPortal* that = static_cast<RemoteDesktopPortal*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to start the remote desktop session: "
                      << error->message;
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  RTC_LOG(LS_INFO) << "Initializing the start of the remote desktop session.";

  Scoped<char> handle;
  g_variant_get_child(variant.get(), 0, "o", handle.receive());
  if (!handle) {
    RTC_LOG(LS_ERROR)
        << "Failed to initialize the start of the remote desktop session.";
    if (that->start_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->start_request_signal_id_);
      that->start_request_signal_id_ = 0;
    }
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  RTC_LOG(LS_INFO) << "Subscribed to the start signal.";
}

// static
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
    that->PortalFailed(static_cast<RequestResponse>(portal_response));
    return;
  }

  // Array of PipeWire streams. See
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  // documentation for <method name="Start">.
  if (g_variant_lookup(response_data.get(), "streams", "a(ua{sv})",
                       iter.receive())) {
    Scoped<GVariant> variant;

    while (g_variant_iter_next(iter.get(), "@(ua{sv})", variant.receive())) {
      uint32_t stream_id;
      uint32_t type;
      Scoped<GVariant> options;

      g_variant_get(variant.get(), "(u@a{sv})", &stream_id, options.receive());
      RTC_DCHECK(options.get());

      if (g_variant_lookup(options.get(), "source_type", "u", &type)) {
        that->capture_source_type_ = static_cast<CaptureSourceType>(type);
      }

      that->screencast_portal_->SetPipewireStreamNodeId(stream_id);
      break;
    }
  }
  that->screencast_portal_->OpenPipeWireRemote();
}

}  // namespace webrtc
