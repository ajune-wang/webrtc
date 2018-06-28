/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ABSL_STR_CAT_H_
#define API_ABSL_STR_CAT_H_

#include <utility>

// TODO(crbug.com/856536): Remove this workaround and use absl::StrCat directly
// when chromium stops polluting code with windows legacy macroses

#ifdef StrCat
#define WEBRTC_ABSLSTRCAT_HACK 1
#pragma push_macro("StrCat")
#undef StrCat
#endif

#include "absl/strings/str_cat.h"

namespace webrtc {
template <typename... Args>
ABSL_MUST_USE_RESULT inline auto AbslStrCat(Args&&... args)
    -> decltype(absl::StrCat(std::forward<Args>(args)...)) {
  return absl::StrCat(std::forward<Args>(args)...);
}

}  // namespace webrtc

#ifdef WEBRTC_ABSLSTRCAT_HACK
#pragma pop_macro("StrCat")
#undef WEBRTC_ABSLSTRCAT_HACK
#endif

#endif  // API_ABSL_STR_CAT_H_
