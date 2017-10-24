/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import <Foundation/Foundation.h>

#import "RTCWrappedNativeVideoDecoder.h"
#import "RTCWrappedNativeVideoEncoder.h"
#import "WebRTC/RTCVideoDecoderVP8.h"
#import "WebRTC/RTCVideoEncoderVP8.h"

#include "modules/video_coding/codecs/vp8/include/vp8.h"

#pragma mark - Encoder

@implementation RTCVideoEncoderVP8 {
  std::unique_ptr<webrtc::VideoEncoder> _wrappedEncoder;
}

// TODO(andersc): Deprecated, to be removed.
+ (id<RTCVideoEncoder>)vp8Encoder {
  return [[self alloc] init];
}

- (instancetype)init {
  if (self = [super init]) {
    _wrappedEncoder.reset(webrtc::VP8Encoder::Create());
  }

  return self;
}

@end

@interface RTCVideoEncoderVP8 (WrappedNative) <RTCWrappedNativeVideoEncoder>
@end

@implementation RTCVideoEncoderVP8 (WrappedNative)

- (std::unique_ptr<webrtc::VideoEncoder>)releaseWrappedEncoder {
  return std::move(_wrappedEncoder);
}

@end

#pragma mark - Decoder

@implementation RTCVideoDecoderVP8 {
  std::unique_ptr<webrtc::VideoDecoder> _wrappedDecoder;
}

// TODO(andersc): Deprecated, to be removed.
+ (id<RTCVideoDecoder>)vp8Decoder {
  return [[self alloc] init];
}

- (instancetype)init {
  if (self = [super init]) {
    _wrappedDecoder.reset(webrtc::VP8Decoder::Create());
  }

  return self;
}

@end

@interface RTCVideoDecoderVP8 (WrappedNative) <RTCWrappedNativeVideoDecoder>
@end

@implementation RTCVideoDecoderVP8 (WrappedNative)

- (std::unique_ptr<webrtc::VideoDecoder>)releaseWrappedDecoder {
  return std::move(_wrappedDecoder);
}

@end
