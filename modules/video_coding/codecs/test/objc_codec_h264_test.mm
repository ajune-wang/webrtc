/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/objc_codec_h264_test.h"

#import "WebRTC/RTCVideoCodecH264.h"
#include "sdk/objc/Framework/Classes/VideoToolbox/objc_video_decoder_factory.h"
#include "sdk/objc/Framework/Classes/VideoToolbox/objc_video_encoder_factory.h"

namespace webrtc {

std::unique_ptr<cricket::WebRtcVideoEncoderFactory> CreateObjCEncoderFactory() {
  RTCVideoEncoderPriorityList *encoderList = [[RTCVideoEncoderPriorityList alloc] init];
  [encoderList addFormat:[RTCVideoEncoderH264 highProfileCodecInfo]
               withClass:[RTCVideoEncoderH264 class]];
  [encoderList addFormat:[RTCVideoEncoderH264 baselineProfileCodecInfo]
               withClass:[RTCVideoEncoderH264 class]];

  id<RTCVideoEncoderFactory> encoderFactory =
      [[RTCDefaultVideoEncoderFactory alloc] initWithEncoderPriorityList:encoderList];
  return std::unique_ptr<cricket::WebRtcVideoEncoderFactory>(
      new ObjCVideoEncoderFactory(encoderFactory));
}

std::unique_ptr<cricket::WebRtcVideoDecoderFactory> CreateObjCDecoderFactory() {
  RTCVideoDecoderPriorityList *decoderList = [[RTCVideoDecoderPriorityList alloc] init];
  [decoderList addFormat:[[RTCVideoCodecInfo alloc] initWithName:@"H264"]
               withClass:[RTCVideoDecoderH264 class]];

  id<RTCVideoDecoderFactory> decoderFactory =
      [[RTCDefaultVideoDecoderFactory alloc] initWithDecoderPriorityList:decoderList];
  return std::unique_ptr<cricket::WebRtcVideoDecoderFactory>(
      new ObjCVideoDecoderFactory(decoderFactory));
}

}  // namespace webrtc
