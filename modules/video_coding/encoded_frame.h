/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_ENCODED_FRAME_H_
#define MODULES_VIDEO_CODING_ENCODED_FRAME_H_

#include <vector>

#include "api/video/encoded_frame.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {
// TODO(https://bugs.webrtc.org/9378): Move references to EncodedFrame directly
// and delete this class.
using VCMEncodedFrame = EncodedFrame;

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_ENCODED_FRAME_H_
