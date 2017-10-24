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
#import "WebRTC/RTCVideoDecoderVP9.h"
#import "WebRTC/RTCVideoEncoderVP9.h"

#include "modules/video_coding/codecs/vp9/include/vp9.h"

#pragma mark - Encoder

@implementation RTCVideoEncoderVP9 {
  std::unique_ptr<webrtc::VideoEncoder> _wrappedEncoder;
}

// TODO(andersc): Deprecated, to be removed.
+ (id<RTCVideoEncoder>)vp9Encoder {
  return [[self alloc] init];
}

- (instancetype)init {
  if (self = [super init]) {
    _wrappedEncoder.reset(webrtc::VP9Encoder::Create());
  }

  return self;
}

@end

@interface RTCVideoEncoderVP9 (WrappedNative) <RTCWrappedNativeVideoEncoder>
@end

@implementation RTCVideoEncoderVP9 (WrappedNative)

- (std::unique_ptr<webrtc::VideoEncoder>)releaseWrappedEncoder {
  return std::move(_wrappedEncoder);
}

@end

#pragma mark - Decoder

@implementation RTCVideoDecoderVP9 {
  std::unique_ptr<webrtc::VideoDecoder> _wrappedDecoder;
}

// TODO(andersc): Deprecated, to be removed.
+ (id<RTCVideoDecoder>)vp9Decoder {
  return [[self alloc] init];
}

- (instancetype)init {
  if (self = [super init]) {
    _wrappedDecoder.reset(webrtc::VP9Decoder::Create());
  }

  return self;
}

@end

@interface RTCVideoDecoderVP9 (WrappedNative) <RTCWrappedNativeVideoDecoder>
@end

@implementation RTCVideoDecoderVP9 (WrappedNative)

- (std::unique_ptr<webrtc::VideoDecoder>)releaseWrappedDecoder {
  return std::move(_wrappedDecoder);
}

@end
