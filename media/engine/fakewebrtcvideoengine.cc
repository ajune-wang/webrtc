/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/fakewebrtcvideoengine.h"

namespace cricket {

bool IsFormatSupported2(
    const std::vector<webrtc::SdpVideoFormat>& supported_formats,
    const webrtc::SdpVideoFormat& format) {
  for (const webrtc::SdpVideoFormat& supported_format : supported_formats) {
    if (IsSameCodec(format.name, format.parameters, supported_format.name,
                    supported_format.parameters)) {
      return true;
    }
  }
  return false;
}

FakeWebRtcVideoDecoder::FakeWebRtcVideoDecoder(
    FakeWebRtcVideoDecoderFactory* factory)
    : num_frames_received_(0), factory_(factory) {}

FakeWebRtcVideoDecoder::~FakeWebRtcVideoDecoder() {
  if (factory_) {
    factory_->DecoderDestroyed(this);
  }
}

FakeWebRtcVideoEncoder::FakeWebRtcVideoEncoder(
    FakeWebRtcVideoEncoderFactory* factory)
    : init_encode_event_(false, false),
      num_frames_encoded_(0),
      factory_(factory) {}

FakeWebRtcVideoEncoder::~FakeWebRtcVideoEncoder() {
  if (factory_) {
    factory_->EncoderDestroyed(this);
  }
}

}  // namespace cricket
