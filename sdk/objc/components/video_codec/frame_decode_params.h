/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCEncodedImage.h"
#import "RTCVideoDecoder.h"

// Struct that we pass to the decoder per frame to decode. We receive it again
// in the decoder callback.
struct FrameDecodeParams {
  FrameDecodeParams(RTCEncodedImage* image,
                    RTCVideoDecoderCallback cb,
                    int64_t ts)
      : encoded_image(image), callback(cb), timestamp(ts) {}
  // Store a pointer to maintain a reference to the RTCEncodedImage object.
  RTCEncodedImage* encoded_image;
  RTCVideoDecoderCallback callback;
  int64_t timestamp;
};
