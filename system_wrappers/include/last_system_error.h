/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef SYSTEM_WRAPPERS_INCLUDE_LAST_SYSTEM_ERROR_H_
#define SYSTEM_WRAPPERS_INCLUDE_LAST_SYSTEM_ERROR_H_

#include <errno.h>

namespace rtc {

#if defined(WEBRTC_WIN)
#define RTC_LAST_SYSTEM_ERROR \
  (::GetLastError())
#elif defined(__native_client__) && __native_client__
#define RTC_LAST_SYSTEM_ERROR \
  (0)
#elif defined(WEBRTC_POSIX)
#define RTC_LAST_SYSTEM_ERROR \
  (errno)
#endif  // WEBRTC_WIN

}  // namespace rtc

#endif  // SYSTEM_WRAPPERS_INCLUDE_LAST_SYSTEM_ERROR_H_
