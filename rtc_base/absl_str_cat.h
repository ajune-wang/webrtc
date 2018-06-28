/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_ABSL_STR_CAT_H_
#define RTC_BASE_ABSL_STR_CAT_H_

// The Chromium Windows builds #define a StrCat macro that prevents us from
// using absl::StrCat. Include this header instead as a workaround until that
// issue is resolved, see
// https://bugs.chromium.org/p/chromium/issues/detail?id=856536
//
// Do NOT include this in .h files, as that could leak the #undef outside of
// WebRTC.

#undef StrCat
#define StrCat StrCat
#include "absl/strings/str_cat.h"

#endif  // RTC_BASE_ABSL_STR_CAT_H_
