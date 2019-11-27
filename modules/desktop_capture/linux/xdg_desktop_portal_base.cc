/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/xdg_desktop_portal_base.h"

#include <gio/gunixfdlist.h>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

const char kDesktopBusName[] = "org.freedesktop.portal.Desktop";
const char kDesktopObjectPath[] = "/org/freedesktop/portal/desktop";
const char kDesktopRequestObjectPath[] =
    "/org/freedesktop/portal/desktop/request";
const char kSessionInterfaceName[] = "org.freedesktop.portal.Session";
const char kRequestInterfaceName[] = "org.freedesktop.portal.Request";
const char kScreenCastInterfaceName[] = "org.freedesktop.portal.ScreenCast";

ConnectionData::ConnectionData(int32_t web_id) {
  id_ = web_id;
}

ConnectionData::~ConnectionData() {
  if (start_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_, start_request_signal_id_);
  }
  if (sources_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         sources_request_signal_id_);
  }
  if (session_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         session_request_signal_id_);
  }

  if (session_handle_) {
    GDBusMessage* message = g_dbus_message_new_method_call(
        kDesktopBusName, session_handle_, kSessionInterfaceName, "Close");
    if (message) {
      GError* error = nullptr;
      g_dbus_connection_send_message(connection_, message,
                                     G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                     /*out_serial=*/nullptr, &error);
      if (error) {
        RTC_LOG(LS_ERROR) << "Failed to close the session: " << error->message;
        g_error_free(error);
      }
      g_object_unref(message);
    }
  }

  g_free(start_handle_);
  g_free(sources_handle_);
  g_free(session_handle_);
  g_free(portal_handle_);

  if (proxy_) {
    g_clear_object(&proxy_);
  }

  // Restore to initial values
  connection_ = nullptr;
  proxy_ = nullptr;
  portal_handle_ = nullptr;
  session_handle_ = nullptr;
  sources_handle_ = nullptr;
  start_handle_ = nullptr;
  session_request_signal_id_ = 0;
  sources_request_signal_id_ = 0;
  start_request_signal_id_ = 0;

  id_ = 0;
  stream_id_ = 0;
  pw_fd_ = -1;
  portal_init_failed_ = false;
}

XdgDesktopPortalBase::XdgDesktopPortalBase() {}

XdgDesktopPortalBase::~XdgDesktopPortalBase() {}

// static
rtc::scoped_refptr<XdgDesktopPortalBase> XdgDesktopPortalBase::CreateDefault() {
  return new XdgDesktopPortalBase();
}

void XdgDesktopPortalBase::InitPortal(rtc::Callback1<void, bool> callback,
                                      int32_t web_id,
                                      CaptureSourceType type) {
  auto connection_data = std::make_shared<ConnectionData>(web_id);
  connection_data->callback_ = callback;
  connection_data->capture_source_type_ = type;

  connection_data_map_.insert({web_id, connection_data});

  g_dbus_proxy_new_for_bus(
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, /*info=*/nullptr,
      kDesktopBusName, kDesktopObjectPath, kScreenCastInterfaceName,
      /*cancellable=*/nullptr,
      reinterpret_cast<GAsyncReadyCallback>(OnProxyRequested),
      new UserData(web_id, this));
}

guint32 XdgDesktopPortalBase::GetStreamID(int32_t id) const {
  int32_t valid_id = id ? id : current_connection_id_;

  auto connection_data = GetConnectionData(valid_id);
  RTC_CHECK(connection_data);

  return connection_data->stream_id_;
}

DesktopSize XdgDesktopPortalBase::GetDesktopSize(int32_t id) const {
  int32_t valid_id = id ? id : current_connection_id_;

  auto connection_data = GetConnectionData(valid_id);
  RTC_CHECK(connection_data);

  return connection_data->desktop_size_;
}

gint32 XdgDesktopPortalBase::GetPipeWireFd(int32_t id) const {
  int32_t valid_id = id ? id : current_connection_id_;

  auto connection_data = GetConnectionData(valid_id);
  RTC_CHECK(connection_data);

  return connection_data->pw_fd_;
}

std::shared_ptr<ConnectionData> XdgDesktopPortalBase::GetConnectionData(
    int32_t id) const {
  int32_t valid_id = id ? id : current_connection_id_;

  auto search = connection_data_map_.find(valid_id);
  if (search != connection_data_map_.end()) {
    return search->second;
  } else {
    return nullptr;
  }
}

void XdgDesktopPortalBase::SetCurrentConnectionId(int32_t id) {
  current_connection_id_ = id;
}

guint XdgDesktopPortalBase::SetupRequestResponseSignal(
    const gchar* object_path,
    GDBusSignalCallback callback,
    UserData* data) {
  auto connection_data = GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  return g_dbus_connection_signal_subscribe(
      connection_data->connection_, kDesktopBusName, kRequestInterfaceName,
      "Response", object_path, /*arg0=*/nullptr,
      G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, callback, data,
      /*user_data_free_func=*/nullptr);
}

// static
void XdgDesktopPortalBase::OnProxyRequested(GObject* /*object*/,
                                            GAsyncResult* result,
                                            gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* that = data->GetXdgDesktopPortalBase();
  RTC_CHECK(that);

  auto connection_data = that->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  GError* error = nullptr;
  connection_data->proxy_ = g_dbus_proxy_new_finish(result, &error);
  if (!connection_data->proxy_) {
    RTC_LOG(LS_ERROR) << "Failed to create a proxy for the screen cast portal: "
                      << error->message;
    g_error_free(error);
    connection_data->portal_init_failed_ = true;
    connection_data->callback_(false);
    return;
  }
  connection_data->connection_ =
      g_dbus_proxy_get_connection(connection_data->proxy_);

  RTC_LOG(LS_INFO) << "Created proxy for the screen cast portal.";
  that->SessionRequest(data);
}

// static
gchar* XdgDesktopPortalBase::PrepareSignalHandle(GDBusConnection* connection,
                                                 const gchar* token) {
  gchar* sender = g_strdup(g_dbus_connection_get_unique_name(connection) + 1);
  for (int i = 0; sender[i]; i++) {
    if (sender[i] == '.') {
      sender[i] = '_';
    }
  }

  gchar* handle = g_strconcat(kDesktopRequestObjectPath, "/", sender, "/",
                              token, /*end of varargs*/ nullptr);
  g_free(sender);

  return handle;
}

void XdgDesktopPortalBase::SessionRequest(UserData* data) {
  auto connection_data = GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  GVariantBuilder builder;
  gchar* variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string =
      g_strdup_printf("webrtc_session%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "session_handle_token",
                        g_variant_new_string(variant_string));
  g_free(variant_string);
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string));

  connection_data->portal_handle_ =
      PrepareSignalHandle(connection_data->connection_, variant_string);
  connection_data->session_request_signal_id_ = SetupRequestResponseSignal(
      connection_data->portal_handle_, OnSessionRequestResponseSignal, data);
  g_free(variant_string);

  RTC_LOG(LS_INFO) << "Screen cast session requested.";
  g_dbus_proxy_call(connection_data->proxy_, "CreateSession",
                    g_variant_new("(a{sv})", &builder), G_DBUS_CALL_FLAGS_NONE,
                    /*timeout=*/-1, /*cancellable=*/nullptr,
                    reinterpret_cast<GAsyncReadyCallback>(OnSessionRequested),
                    data);
}

// static
void XdgDesktopPortalBase::OnSessionRequested(GDBusConnection* connection,
                                              GAsyncResult* result,
                                              gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* that = data->GetXdgDesktopPortalBase();
  RTC_CHECK(that);

  auto connection_data = that->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  GError* error = nullptr;
  GVariant* variant =
      g_dbus_proxy_call_finish(connection_data->proxy_, result, &error);
  if (!variant) {
    RTC_LOG(LS_ERROR) << "Failed to create a screen cast session: "
                      << error->message;
    g_error_free(error);
    connection_data->portal_init_failed_ = true;
    connection_data->callback_(false);
    return;
  }
  RTC_LOG(LS_INFO) << "Initializing the screen cast session.";

  gchar* handle = nullptr;
  g_variant_get_child(variant, 0, "o", &handle);
  g_variant_unref(variant);
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the screen cast session.";
    if (connection_data->session_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(
          connection, connection_data->session_request_signal_id_);
      connection_data->session_request_signal_id_ = 0;
    }
    connection_data->portal_init_failed_ = true;
    connection_data->callback_(false);
    return;
  }

  g_free(handle);

  RTC_LOG(LS_INFO) << "Subscribing to the screen cast session.";
}

// static
void XdgDesktopPortalBase::OnSessionRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* that = data->GetXdgDesktopPortalBase();
  RTC_CHECK(that);

  auto connection_data = that->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  RTC_LOG(LS_INFO)
      << "Received response for the screen cast session subscription.";

  guint32 portal_response;
  GVariant* response_data;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, &response_data);
  g_variant_lookup(response_data, "session_handle", "s",
                   &connection_data->session_handle_);
  g_variant_unref(response_data);

  if (!connection_data->session_handle_ || portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to request the screen cast session subscription.";
    connection_data->portal_init_failed_ = true;
    connection_data->callback_(false);
    return;
  }

  that->SourcesRequest(data);
}

void XdgDesktopPortalBase::SourcesRequest(UserData* data) {
  auto connection_data = GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  GVariantBuilder builder;
  gchar* variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  // We want to record monitor content.
  g_variant_builder_add(
      &builder, "{sv}", "types",
      g_variant_new_uint32(connection_data->capture_source_type_));
  // We don't want to allow selection of multiple sources.
  g_variant_builder_add(&builder, "{sv}", "multiple",
                        g_variant_new_boolean(false));
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string));

  connection_data->sources_handle_ =
      PrepareSignalHandle(connection_data->connection_, variant_string);
  connection_data->sources_request_signal_id_ = SetupRequestResponseSignal(
      connection_data->sources_handle_, OnSourcesRequestResponseSignal, data);
  g_free(variant_string);

  RTC_LOG(LS_INFO) << "Requesting sources from the screen cast session.";
  g_dbus_proxy_call(
      connection_data->proxy_, "SelectSources",
      g_variant_new("(oa{sv})", connection_data->session_handle_, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, /*cancellable=*/nullptr,
      reinterpret_cast<GAsyncReadyCallback>(OnSourcesRequested), data);
}

// static
void XdgDesktopPortalBase::OnSourcesRequested(GDBusConnection* connection,
                                              GAsyncResult* result,
                                              gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* that = data->GetXdgDesktopPortalBase();
  RTC_CHECK(that);

  auto connection_data = that->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  GError* error = nullptr;
  GVariant* variant =
      g_dbus_proxy_call_finish(connection_data->proxy_, result, &error);
  if (!variant) {
    RTC_LOG(LS_ERROR) << "Failed to request the sources: " << error->message;
    g_error_free(error);
    connection_data->portal_init_failed_ = true;
    connection_data->callback_(false);
    return;
  }

  RTC_LOG(LS_INFO) << "Sources requested from the screen cast session.";

  gchar* handle = nullptr;
  g_variant_get_child(variant, 0, "o", &handle);
  g_variant_unref(variant);
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the screen cast session.";
    if (connection_data->sources_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(
          connection, connection_data->sources_request_signal_id_);
      connection_data->sources_request_signal_id_ = 0;
    }
    connection_data->portal_init_failed_ = true;
    connection_data->callback_(false);
    return;
  }

  g_free(handle);

  RTC_LOG(LS_INFO) << "Subscribed to sources signal.";
}

// static
void XdgDesktopPortalBase::OnSourcesRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* that = data->GetXdgDesktopPortalBase();
  RTC_CHECK(that);

  auto connection_data = that->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  RTC_LOG(LS_INFO) << "Received sources signal from session.";

  guint32 portal_response;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, nullptr);
  if (portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to select sources for the screen cast session.";
    connection_data->portal_init_failed_ = true;
    connection_data->callback_(false);
    return;
  }

  that->StartRequest(data);
}

void XdgDesktopPortalBase::StartRequest(UserData* data) {
  auto connection_data = GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  GVariantBuilder builder;
  gchar* variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string));

  connection_data->start_handle_ =
      PrepareSignalHandle(connection_data->connection_, variant_string);
  connection_data->start_request_signal_id_ = SetupRequestResponseSignal(
      connection_data->start_handle_, OnStartRequestResponseSignal, data);
  g_free(variant_string);

  // "Identifier for the application window", this is Wayland, so not "x11:...".
  const gchar parent_window[] = "";

  RTC_LOG(LS_INFO) << "Starting the screen cast session.";
  g_dbus_proxy_call(
      connection_data->proxy_, "Start",
      g_variant_new("(osa{sv})", connection_data->session_handle_,
                    parent_window, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, /*cancellable=*/nullptr,
      reinterpret_cast<GAsyncReadyCallback>(OnStartRequested), data);
}

// static
void XdgDesktopPortalBase::OnStartRequested(GDBusConnection* connection,
                                            GAsyncResult* result,
                                            gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* that = data->GetXdgDesktopPortalBase();
  RTC_CHECK(that);

  auto connection_data = that->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  GError* error = nullptr;
  GVariant* variant =
      g_dbus_proxy_call_finish(connection_data->proxy_, result, &error);
  if (!variant) {
    RTC_LOG(LS_ERROR) << "Failed to start the screen cast session: "
                      << error->message;
    g_error_free(error);
    connection_data->portal_init_failed_ = true;
    connection_data->callback_(false);
    return;
  }

  RTC_LOG(LS_INFO) << "Initializing the start of the screen cast session.";

  gchar* handle = nullptr;
  g_variant_get_child(variant, 0, "o", &handle);
  g_variant_unref(variant);
  if (!handle) {
    RTC_LOG(LS_ERROR)
        << "Failed to initialize the start of the screen cast session.";
    if (connection_data->start_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(
          connection, connection_data->start_request_signal_id_);
      connection_data->start_request_signal_id_ = 0;
    }
    connection_data->portal_init_failed_ = true;
    connection_data->callback_(false);
    return;
  }

  g_free(handle);

  RTC_LOG(LS_INFO) << "Subscribed to the start signal.";
}

// static
void XdgDesktopPortalBase::OnStartRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* that = data->GetXdgDesktopPortalBase();
  RTC_CHECK(that);

  auto connection_data = that->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  RTC_LOG(LS_INFO) << "Start signal received.";
  guint32 portal_response;
  GVariant* response_data;
  GVariantIter* iter = nullptr;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, &response_data);
  if (portal_response || !response_data) {
    RTC_LOG(LS_ERROR) << "Failed to start the screen cast session.";
    connection_data->portal_init_failed_ = true;
    connection_data->callback_(false);
    return;
  }

  // Array of PipeWire streams. See
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  // documentation for <method name="Start">.
  if (g_variant_lookup(response_data, "streams", "a(ua{sv})", &iter)) {
    GVariant* variant;

    while (g_variant_iter_next(iter, "@(ua{sv})", &variant)) {
      guint32 stream_id;
      gint32 width;
      gint32 height;
      GVariant* options;

      g_variant_get(variant, "(u@a{sv})", &stream_id, &options);
      RTC_DCHECK(options != nullptr);

      g_variant_lookup(options, "size", "(ii)", &width, &height);

      connection_data->desktop_size_.set(width, height);
      connection_data->stream_id_ = stream_id;

      g_variant_unref(options);
      g_variant_unref(variant);
      break;
    }
  }
  g_variant_iter_free(iter);
  g_variant_unref(response_data);

  connection_data->callback_(true);
}

void XdgDesktopPortalBase::OpenPipeWireRemote(
    rtc::Callback2<void, bool, int32_t> pwCallback,
    int32_t id) {
  auto connection_data = GetConnectionData(id);

  if (!connection_data) {
    pwCallback(false, 0);
    return;
  }

  connection_data->pwCallback_ = pwCallback;

  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

  RTC_LOG(LS_INFO) << "Opening the PipeWire remote.";

  g_dbus_proxy_call_with_unix_fd_list(
      connection_data->proxy_, "OpenPipeWireRemote",
      g_variant_new("(oa{sv})", connection_data->session_handle_, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, /*fd_list=*/nullptr,
      /*cancellable=*/nullptr,
      reinterpret_cast<GAsyncReadyCallback>(OnOpenPipeWireRemoteRequested),
      new UserData(id, this));
}

// static
void XdgDesktopPortalBase::OnOpenPipeWireRemoteRequested(
    GDBusConnection* connection,
    GAsyncResult* result,
    gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* that = data->GetXdgDesktopPortalBase();
  RTC_CHECK(that);

  auto connection_data = that->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  GError* error = nullptr;
  GUnixFDList* outlist = nullptr;
  GVariant* variant = g_dbus_proxy_call_with_unix_fd_list_finish(
      connection_data->proxy_, &outlist, result, &error);
  if (!variant) {
    RTC_LOG(LS_ERROR) << "Failed to open the PipeWire remote: "
                      << error->message;
    g_error_free(error);
    connection_data->portal_init_failed_ = true;
    connection_data->pwCallback_(false, 0);
    return;
  }

  gint32 index;
  g_variant_get(variant, "(h)", &index);

  if ((connection_data->pw_fd_ = g_unix_fd_list_get(outlist, index, &error)) ==
      -1) {
    RTC_LOG(LS_ERROR) << "Failed to get file descriptor from the list: "
                      << error->message;
    g_error_free(error);
    g_variant_unref(variant);
    connection_data->portal_init_failed_ = true;
    connection_data->pwCallback_(false, 0);
    return;
  }

  connection_data->pwCallback_(true, connection_data->id_);
  g_variant_unref(variant);
  g_object_unref(outlist);
}

void XdgDesktopPortalBase::CloseConnection(int32_t id) {
  connection_data_map_.erase(id);
}

}  // namespace webrtc
