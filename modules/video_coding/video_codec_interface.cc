/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

FrameEncodingInfo::FrameEncodingInfo() = default;
FrameEncodingInfo::~FrameEncodingInfo() = default;

CodecSpecificInfo::CodecSpecificInfo()
    : codecType(kVideoCodecGeneric), codec_name(nullptr) {
  memset(&codecSpecific, 0, sizeof(codecSpecific));
}

CodecSpecificInfo::~CodecSpecificInfo() = default;

}  // namespace webrtc
