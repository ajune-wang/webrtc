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

#include "absl/strings/str_cat.h"

namespace webrtc {

template <typename... Args>
ABSL_MUST_USE_RESULT inline std::string ConCat(Args&&... args) {
#ifdef StrCat
#define WEBRTC_DISABLED_STRCAT StrCat
#undef StrCat
#endif
  return absl::StrCat(std::forward<Args>(args)...);
#ifdef WEBRTC_DISABLED_StrCat
#define StrCat WEBRTC_DISABLED_StrCat
#undef WEBRTC_DISABLED_StrCat
#endif
}

}  // namespace webrtc

#endif  // RTC_BASE_CON_CAT_H_
