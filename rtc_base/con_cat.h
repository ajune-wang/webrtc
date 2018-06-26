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

namespace webrtc {

template <typename... Args>
ABSL_MUST_USE_RESULT inline std::string ConCat(Args&&... args) {
  return absl::StrCat(std::forward<Args>(args)...);
}

}  // namespace webrtc

#ifdef WEBRTC_DISABLED_STRCAT
#define StrCat WEBRTC_DISABLED_STRCAT
#undef WEBRTC_DISABLED_STRCAT
#endif

#endif  // RTC_BASE_CON_CAT_H_
