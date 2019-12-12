/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ENCODED_FRAME_TRANSFORM_INTERFACE_H_
#define API_ENCODED_FRAME_TRANSFORM_INTERFACE_H_

#include "api/array_view.h"
#include "api/video/encoded_frame.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class TransformedReceivedFrameCallback {
 public:
  virtual void OnTransformedFrame(
      std::unique_ptr<video_coding::EncodedFrame> frame) = 0;

 protected:
  virtual ~TransformedReceivedFrameCallback() = default;
};

class ReceivedFrameTransformInterface {
 public:
  virtual ~ReceivedFrameTransformInterface() = default;

  virtual void RegisterTransformedFrameCallback(
      TransformedReceivedFrameCallback*) = 0;
  virtual void TransformFrame(
      std::unique_ptr<video_coding::EncodedFrame> frame) = 0;
};

}  // namespace webrtc

#endif  // API_ENCODED_FRAME_TRANSFORM_INTERFACE_H_
