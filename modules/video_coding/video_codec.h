/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_VIDEO_CODEC_H_
#define MODULES_VIDEO_CODING_VIDEO_CODEC_H_

#include <string>

namespace webrtc {
namespace webrtc_internal {

struct VideoCodec {
  std::string name;
};

}  // namespace webrtc_internal
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_VIDEO_CODEC_H_
