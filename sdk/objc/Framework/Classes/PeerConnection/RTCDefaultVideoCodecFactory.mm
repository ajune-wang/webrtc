/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodecFactory.h"

#include "rtc_base/logging.h"

#pragma mark - Encoder Factory

@implementation RTCDefaultVideoEncoderFactory {
  NSDictionary<RTCVideoCodecInfo *, Class<RTCVideoEncoder>> *_encoderClasses;
  NSArray<RTCVideoCodecInfo *> *_codecsByPriority;
}

- (instancetype)initWithEncoderClasses:
                    (NSDictionary<RTCVideoCodecInfo *, Class<RTCVideoEncoder>> *)encoderClasses
                              priority:(NSArray<RTCVideoCodecInfo *> *)codecsByPriority {
  if (self = [super init]) {
    if (![[NSSet setWithArray:[encoderClasses allKeys]]
            isEqualToSet:[NSSet setWithArray:codecsByPriority]]) {
      LOG(LS_WARNING) << "Codec priority list should contain all supported codecs.";
    }
    _encoderClasses = encoderClasses;
    _codecsByPriority = codecsByPriority;
  }

  return self;
}

- (id<RTCVideoEncoder>)createEncoder:(RTCVideoCodecInfo *)info {
  Class<RTCVideoEncoder> encoderClass = _encoderClasses[info];
  id<RTCVideoEncoder> encoderInstance = [[(Class)encoderClass alloc] init];
  if ([encoderInstance respondsToSelector:@selector(configureWithCodecInfo:)]) {
    [encoderInstance configureWithCodecInfo:info];
  }
  return encoderInstance;
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  return _codecsByPriority;
}

@end

#pragma mark - Decoder Factory

@implementation RTCDefaultVideoDecoderFactory {
  NSDictionary<RTCVideoCodecInfo *, Class<RTCVideoDecoder>> *_decoderClasses;
  NSArray<RTCVideoCodecInfo *> *_codecsByPriority;
}

- (instancetype)initWithDecoderClasses:
        (NSDictionary<RTCVideoCodecInfo *, Class<RTCVideoDecoder>> *)decoderClasses {
  if (self = [super init]) {
    _decoderClasses = decoderClasses;
  }

  return self;
}

- (id<RTCVideoDecoder>)createDecoder:(RTCVideoCodecInfo *)info {
  Class<RTCVideoDecoder> decoderClass = _decoderClasses[info];
  id<RTCVideoDecoder> decoderInstance = [[(Class)decoderClass alloc] init];
  if ([decoderInstance respondsToSelector:@selector(configureWithCodecInfo:)]) {
    [decoderInstance configureWithCodecInfo:info];
  }
  return decoderInstance;
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  return [_decoderClasses allKeys];
}

@end
