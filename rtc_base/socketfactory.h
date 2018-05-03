/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SOCKETFACTORY_H_
#define RTC_BASE_SOCKETFACTORY_H_

#include "rtc_base/asyncsocket.h"
#include "rtc_base/socket.h"

namespace rtc {

class SocketFactory {
public:
  virtual ~SocketFactory() {}

  // TODO(bugs.webrtc.org/9237): Remove once AsyncSocket has merged into Socket.
  virtual Socket* CreateSocket(int family, int type);
  // Returns a new socket for nonblocking communication.  The type can be
  // SOCK_DGRAM and SOCK_STREAM.
  virtual AsyncSocket* CreateAsyncSocket(int family, int type) = 0;
};

inline Socket* SocketFactory::CreateSocket(int family, int type) {
  RTC_NOTREACHED();
  return nullptr;
}

} // namespace rtc

#endif // RTC_BASE_SOCKETFACTORY_H_
