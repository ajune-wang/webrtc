/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_UNITS_TYPE_TRAIT_TOOLS_H_
#define API_UNITS_TYPE_TRAIT_TOOLS_H_
#include <type_traits>

namespace webrtc {
template <typename T,
          typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
struct ForInt {
  using type = T;
};

template <typename T,
          typename std::enable_if<std::is_floating_point<T>::value>::type* =
              nullptr>
struct ForFloat {
  using type = T;
};

}  // namespace webrtc
#endif  // API_UNITS_TYPE_TRAIT_TOOLS_H_
