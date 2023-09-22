/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_SYSTEM_RTC_DEPRECATED_H_
#define RTC_BASE_SYSTEM_RTC_DEPRECATED_H_

#ifdef WEBRTC_CHROMIUM_BUILD
#define RTC_DEPRECATED(message)
#else
#define RTC_DEPRECATED(message) [[deprecated(message)]]
#endif

#endif  // RTC_BASE_SYSTEM_RTC_DEPRECATED_H_
