/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CRICKET_NAMESPACE_COMPATIBILITY_H_
#define RTC_BASE_CRICKET_NAMESPACE_COMPATIBILITY_H_

#if defined(WEBRTC_NAMESPACES_BACKWARDS_COMPATIBILITY)
namespace webrtc {}
namespace cricket = webrtc;
#endif  // defined(WEBRTC_NAMESPACES_BACKWARDS_COMPATIBILITY)

#endif  // RTC_BASE_CRICKET_NAMESPACE_COMPATIBILITY_H_
