/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/encoded_frame.h"

#include "absl/types/optional.h"

namespace webrtc {
EncodedFrame::~EncodedFrame() {
  Reset();
}

absl::optional<Timestamp> EncodedFrame::ReceivedTimestamp() const {
  return ReceivedTime() >= 0
             ? absl::make_optional(Timestamp::Millis(ReceivedTime()))
             : absl::nullopt;
}

absl::optional<Timestamp> EncodedFrame::RenderTimestamp() const {
  return RenderTimeMs() >= 0
             ? absl::make_optional(Timestamp::Millis(RenderTimeMs()))
             : absl::nullopt;
}

bool EncodedFrame::delayed_by_retransmission() const {
  return false;
}

void EncodedFrame::Reset() {
  SetTimestamp(0);
  SetSpatialIndex(absl::nullopt);
  _renderTimeMs = -1;
  _payloadType = 0;
  _frameType = VideoFrameType::kVideoFrameDelta;
  _encodedWidth = 0;
  _encodedHeight = 0;
  _missingFrame = false;
  set_size(0);
  _codec = kVideoCodecGeneric;
  rotation_ = kVideoRotation_0;
  content_type_ = VideoContentType::UNSPECIFIED;
  timing_.flags = VideoSendTiming::kInvalid;
}

}  // namespace webrtc
