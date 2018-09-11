/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_BUILD_SYMBOL_VISIBILITY_H_
#define RTC_BASE_BUILD_SYMBOL_VISIBILITY_H_

// This code is copied from V8 and redacted to be used in WebRTC.
// Source:
// https://cs.chromium.org/chromium/src/v8/include/v8.h?l=31-59&rcl=b0af30948505b68c843b538e109ab378d3750e37
//
// RTC_PUBLIC is used to mark symbols as exported or imported when WebRTC is
// built or used as a shared library.
// When WebRTC is built as a static library the RTC_PUBLIC macro expands to
// nothing.
//
// When building a shared library, make sure the macro WEBRTC_EXPORT_SYMBOLS is
// defined.
// When using a WebRTC shared library, make sure the macro WEBRTC_IMPORT_SYMBOLS
// is defined.
// When building or using WebRTC as a static library there is no need to define
// any of these macros.

#if WEBRTC_EXPORT_SYMBOLS && WEBRTC_IMPORT_SYMBOLS
#error "WEBRTC_EXPORT_SYMBOLS and WEBRTC_IMPORT_SYMBOLS should never be defined"
#error "together."
#endif

#ifdef WEBRTC_WIN

#if WEBRTC_EXPORT_SYMBOLS
#define RTC_PUBLIC __declspec(dllexport)
#elif WEBRTC_IMPORT_SYMBOLS
#define RTC_PUBLIC __declspec(dllimport)
#endif

#else

#if __has_attribute(visibility) && WEBRTC_EXPORT_SYMBOLS
#define RTC_PUBLIC __attribute__((visibility("default")))
#endif

#endif  // WEBRTC_WIN

#ifndef RTC_PUBLIC
#define RTC_PUBLIC
#endif

#endif  // RTC_BASE_BUILD_SYMBOL_VISIBILITY_H_
