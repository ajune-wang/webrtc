/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/default_socket_server.h"

#include "absl/memory/memory.h"

#ifdef __native_client__
#include "rtc_base/nullsocketserver.h"
#else
#include "rtc_base/physicalsocketserver.h"
#endif

namespace rtc {

std::unique_ptr<SocketServer> CreateDefaultSocketServer() {
#ifdef __native_client__
  return absl::make_unique<NullSocketServer>();
#else
  return absl::make_unique<PhysicalSocketServer>();
#endif
}

}  // namespace rtc
