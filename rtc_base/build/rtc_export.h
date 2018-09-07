/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_BUILD_RTC_EXPORT_H_
#define RTC_BASE_BUILD_RTC_EXPORT_H_

// This code is copied from V8 and redacted to be used in WebRTC.
// Source: https://cs.chromium.org/chromium/src/v8/include/v8.h?l=31-59&rcl=b0af30948505b68c843b538e109ab378d3750e37

#ifdef WEBRTC_WIN

// Setup for Windows DLL export/import.
// When building the WEBRTC DLL the BUILDING_WEBRTC_SHARED needs to be defined.
// When building a program which uses the WEBRTC DLL USING_WEBRTC_SHARED needs
// to be defined.
// When either building the WEBRTC static library or building a program which
// uses the WEBRTC static library neither BUILDING_WEBRTC_SHARED nor
// USING_WEBRTC_SHARED should be defined.

#ifdef BUILDING_WEBRTC_SHARED
#define RTC_EXPORT __declspec(dllexport)
#elif USING_WEBRTC_SHARED
#define RTC_EXPORT __declspec(dllimport)
#else
#define RTC_EXPORT
#endif

#else

#if __has_attribute(visibility)
# if BUILDING_WEBRTC_SHARED
# define RTC_EXPORT __attribute__((visibility("default")))
# else
# define RTC_EXPORT
# endif
#else
# define RTC_EXPORT
#endif

#endif

#endif  // RTC_BASE_BUILD_RTC_EXPORT_H_
