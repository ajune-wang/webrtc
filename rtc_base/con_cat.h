/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CON_CAT_H_
#define RTC_BASE_CON_CAT_H_

#include <string>
#include <utility>

#ifdef StrCat
#define WEBRTC_DISABLED_STRCAT StrCat
#undef StrCat
#endif

#include "absl/strings/str_cat.h"

// Chromium defines StrCat as StrCatW. introduce that forward function too
// so that calling StrCat would work even when compiled with chromium headers
// included
// Do call this function directly, always write StrCat
// another hacky idea...
namespace absl {
template <typename... Args>
ABSL_MUST_USE_RESULT inline std::string StrCatW(Args&&... args) {
  return absl::StrCat(std::forward<Args>(args)...);
}
}  // namespace absl

#ifdef WEBRTC_DISABLED_STRCAT
#define StrCat WEBRTC_DISABLED_STRCAT
#undef WEBRTC_DISABLED_STRCAT
#endif

#endif  // RTC_BASE_CON_CAT_H_
