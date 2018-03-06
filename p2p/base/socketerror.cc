/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/socketerror.h"

#if defined(WEBRTC_POSIX)
#include "rtc_base/safe_strerror.h"
#endif

namespace rtc {

std::ostream& operator<<(std::ostream& stream, const SocketError& error) {
  stream << error.code();
#ifdef WEBRTC_POSIX
  stream << " (" << safe_strerror(error.code()) << ")";
#endif
  return stream;
}

}  // namespace rtc
