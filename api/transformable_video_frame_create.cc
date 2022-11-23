/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transformable_video_frame_create.h"

#include <memory>

#include "modules/rtp_rtcp/source/rtp_sender_video_frame_transformer_delegate.h"

namespace webrtc {

std::unique_ptr<TransformableVideoFrameInterface>
CreateTransformableVideoFrameForSender(int payload_type,
                                       uint32_t rtp_timestamp,
                                       uint32_t ssrc) {
  return CreateTransformableVideoFrameForSender(payload_type, rtp_timestamp,
                                                ssrc);
}

}  // namespace webrtc
