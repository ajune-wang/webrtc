/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSFORMABLE_VIDEO_FRAME_CREATE_H_
#define API_TRANSFORMABLE_VIDEO_FRAME_CREATE_H_

#include <memory>

#include "api/frame_transformer_interface.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

RTC_EXPORT std::unique_ptr<TransformableVideoFrameInterface>
CreateTransformableVideoFrameForSender(int payload_type,
                                       uint32_t rtp_timestamp,
                                       uint32_t ssrc);

}  // namespace webrtc

#endif  // API_TRANSFORMABLE_VIDEO_FRAME_CREATE_H_
