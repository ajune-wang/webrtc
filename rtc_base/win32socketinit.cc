/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/win32socketinit.h"

#include "rtc_base/checks.h"
#include "rtc_base/win32.h"

namespace rtc {

#if defined(WEBRTC_WIN)
class WinsockInitializer {
 public:
  WinsockInitializer() {
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(1, 0);
    err_ = WSAStartup(wVersionRequested, &wsaData);
  }
  ~WinsockInitializer() {
    if (!err_)
      WSACleanup();
  }
  int error() { return err_; }

 private:
  int err_;
};
#endif

// If Winsock is not already initialized, applications that use WebRTC
// will have to call this function.
void EnsureWinsockInit() {
  // winsock_init is intentionally leaked in order to avoid an exit time
  // destructor.
  static WinsockInitializer* winsock_init = new WinsockInitializer();
  RTC_CHECK_EQ(winsock_init->error(), 0);
}

}  // namespace rtc
