/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_XDG_DESKTOP_PORTAL_BASE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_XDG_DESKTOP_PORTAL_BASE_H_

#include <gio/gio.h>
#define typeof __typeof__

#include <map>
#include <memory>

#include "modules/desktop_capture/desktop_geometry.h"

#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "rtc_base/callback.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

class UserData;
class ConnectionData;

class XdgDesktopPortalBase : public rtc::RefCountedBase {
 public:
  enum CaptureSourceType { Screen = 1, Window, All };

  XdgDesktopPortalBase();
  ~XdgDesktopPortalBase();

  static rtc::scoped_refptr<XdgDesktopPortalBase> CreateDefault();

  void InitPortal(rtc::Callback1<void, bool> callback,
                  int32_t web_id,
                  CaptureSourceType type = All);
  void OpenPipeWireRemote(rtc::Callback2<void, bool, int32_t> pwCallback,
                          int32_t id = 0);
  void CloseConnection(int32_t id);

  guint32 GetStreamID(int32_t id = 0) const;
  DesktopSize GetDesktopSize(int32_t id = 0) const;
  gint32 GetPipeWireFd(int32_t id = 0) const;

  // FIXME I couldn't find a way how to pass the id through the DesktopCapturer
  // itself
  void SetCurrentConnectionId(int32_t id);

  std::shared_ptr<ConnectionData> GetConnectionData(int32_t id) const;

 private:
  int32_t current_connection_id_ = 0;
  std::map<int32_t, std::shared_ptr<ConnectionData>> connection_data_map_;

  guint SetupRequestResponseSignal(const gchar* object_path,
                                   GDBusSignalCallback callback,
                                   UserData* data);

  static void OnProxyRequested(GObject* object,
                               GAsyncResult* result,
                               gpointer user_data);

  static gchar* PrepareSignalHandle(GDBusConnection* connection,
                                    const gchar* token);

  void SessionRequest(UserData* data);
  static void OnSessionRequested(GDBusConnection* connection,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnSessionRequestResponseSignal(GDBusConnection* connection,
                                             const gchar* sender_name,
                                             const gchar* object_path,
                                             const gchar* interface_name,
                                             const gchar* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);

  void SourcesRequest(UserData* data);
  static void OnSourcesRequested(GDBusConnection* connection,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnSourcesRequestResponseSignal(GDBusConnection* connection,
                                             const gchar* sender_name,
                                             const gchar* object_path,
                                             const gchar* interface_name,
                                             const gchar* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);

  void StartRequest(UserData* data);
  static void OnStartRequested(GDBusConnection* connection,
                               GAsyncResult* result,
                               gpointer user_data);
  static void OnStartRequestResponseSignal(GDBusConnection* connection,
                                           const gchar* sender_name,
                                           const gchar* object_path,
                                           const gchar* interface_name,
                                           const gchar* signal_name,
                                           GVariant* parameters,
                                           gpointer user_data);

  static void OnOpenPipeWireRemoteRequested(GDBusConnection* connection,
                                            GAsyncResult* result,
                                            gpointer user_data);
};

class ConnectionData {
 public:
  explicit ConnectionData(int32_t web_id);
  ~ConnectionData();

  bool operator=(int32_t id) { return id_ == id; }

  gint32 pw_fd_ = -1;

  XdgDesktopPortalBase::CaptureSourceType capture_source_type_ =
      XdgDesktopPortalBase::CaptureSourceType::All;

  GDBusConnection* connection_ = nullptr;
  GDBusProxy* proxy_ = nullptr;
  gchar* portal_handle_ = nullptr;
  gchar* session_handle_ = nullptr;
  gchar* sources_handle_ = nullptr;
  gchar* start_handle_ = nullptr;
  guint session_request_signal_id_ = 0;
  guint sources_request_signal_id_ = 0;
  guint start_request_signal_id_ = 0;

  DesktopSize desktop_size_ = {};
  guint32 stream_id_ = 0;

  int32_t id_;

  bool portal_init_failed_ = false;
  rtc::Callback1<void, bool> callback_;
  rtc::Callback2<void, bool, int32_t> pwCallback_;
};

// Structure which is used as user_data property in GLib async calls, where we
// need to pass two values. One is ID of the web page requesting screen sharing
// and the other is pointer to the XdgDesktopPortalBase object.
class UserData {
 public:
  UserData(int32_t id, XdgDesktopPortalBase* xdp) {
    data_id_ = id;
    xdp_ = xdp;
  }

  int32_t GetDataId() const { return data_id_; }
  XdgDesktopPortalBase* GetXdgDesktopPortalBase() const { return xdp_; }

 private:
  int32_t data_id_ = 0;
  XdgDesktopPortalBase* xdp_ = nullptr;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_XDG_DESKTOP_PORTAL_BASE_H_
