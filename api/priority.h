/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_PRIORITY_H_
#define API_PRIORITY_H_

#include <stdint.h>

namespace webrtc {

// GENERATED_JAVA_ENUM_PACKAGE: org.webrtc
enum class Priority : uint16_t {
  kVeryLow = 128,
  kLow = 256,
  kMedium = 512,
  kHigh = 1024,
};

}  // namespace webrtc

#endif  // API_PRIORITY_H_
