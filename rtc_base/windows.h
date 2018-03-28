/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_WINDOWS_H_
#define RTC_BASE_WINDOWS_H_

#ifndef WEBRTC_WIN
#error "Only #include this header in Windows builds"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Make sure we don't get min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#endif  // RTC_BASE_WINDOWS_H_
