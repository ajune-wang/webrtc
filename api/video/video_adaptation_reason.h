/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_ADAPTATION_REASON_H_
#define API_VIDEO_VIDEO_ADAPTATION_REASON_H_

#include <cstddef>

namespace webrtc {

enum class VideoAdaptationReason : size_t { kQuality = 0, kCpu = 1 };

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_ADAPTATION_REASON_H_
