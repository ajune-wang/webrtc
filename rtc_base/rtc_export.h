/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_RTC_EXPORT_H_
#define RTC_BASE_RTC_EXPORT_H_

#ifdef WEBRTC_WIN

#ifdef BUILDING_WEBRTC_SHARED
#define RTC_EXPORT __declspec(dllexport)
#elif USING_WEBRTC_SHARED
#define RTC_EXPORT __declspec(dllimport)
#else
#define RTC_EXPORT
#endif

#else

#if __has_attribute(visibility)
#if BUILDING_WEBRTC_SHARED
#define RTC_EXPORT __attribute__((visibility("default")))
#else
#define RTC_EXPORT
#endif
#else
#define RTC_EXPORT
#endif

#endif

#endif  // RTC_BASE_RTC_EXPORT_H_
