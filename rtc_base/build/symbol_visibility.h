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

// RTC_PUBLIC is used to mark symbols as exported or imported when WebRTC is
// built or used as a shared library.
// When WebRTC is built as a static library the RTC_PUBLIC macro expands to
// nothing.
//
// When building a shared library, make sure the macro WEBRTC_EXPORT_SYMBOLS is
// defined.

#if WEBRTC_SHARED_LIBRARY

#ifdef WEBRTC_WIN

#ifdef WEBRTC_IMPLEMENTATION
#define RTC_PUBLIC __declspec(dllexport)
#else
#define RTC_PUBLIC __declspec(dllimport)
#endif

#else  // WEBRTC_WIN

#if __has_attribute(visibility) && defined(WEBRTC_IMPLEMENTATION)
#define RTC_PUBLIC __attribute__((visibility("default")))
#endif

#endif  // WEBRTC_WIN

#endif  // WEBRTC_SHARED_LIBRARY

#ifndef RTC_PUBLIC
#define RTC_PUBLIC
#endif

#endif  // RTC_BASE_BUILD_SYMBOL_VISIBILITY_H_
