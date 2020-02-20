/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_FRAME_TRANSFORMER_INTERFACE_H_
#define API_FRAME_TRANSFORMER_INTERFACE_H_

#include <memory>
#include <vector>

#include "api/video/encoded_frame.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class TransformedFrameCallback : public rtc::RefCountInterface {
 public:
  virtual void OnTransformedFrame(
      std::unique_ptr<video_coding::EncodedFrame> frame) = 0;

 protected:
  ~TransformedFrameCallback() override = default;
};

class FrameTransformerInterface : public rtc::RefCountInterface {
 public:
  virtual void RegisterTransformedFrameCallback(
      rtc::scoped_refptr<TransformedFrameCallback>) = 0;
  virtual void UnregisterTransformedFrameCallback() = 0;
  virtual void TransformFrame(std::unique_ptr<video_coding::EncodedFrame> frame,
                              std::vector<uint8_t> additional_data,
                              uint32_t ssrc = 0) = 0;

 protected:
  ~FrameTransformerInterface() override = default;
};

}  // namespace webrtc

#endif  // API_FRAME_TRANSFORMER_INTERFACE_H_
