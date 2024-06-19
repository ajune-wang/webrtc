/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "RTCVideoToolboxDecoder.h"

@interface RTCVideoToolboxDecoder () {
}
@property(nonatomic, readonly) CMVideoFormatDescriptionRef videoFormat;
@property(nonatomic, readonly) VTDecompressionSessionRef decompressionSession;
@property(nonatomic, readonly) RTCVideoDecoderCallback callback;
@property(nonatomic, readonly) OSStatus error;

- (int)resetDecompressionSession:(NSDictionary *)decoderConfig attribute:(NSDictionary *)attributes;
- (void)setVideoFormat:(CMVideoFormatDescriptionRef)videoFormat;

@end
