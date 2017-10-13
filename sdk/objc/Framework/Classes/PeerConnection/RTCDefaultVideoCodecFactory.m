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

#pragma mark - Codec Priority List

@implementation RTCVideoCodecPriorityList {
  NSMutableDictionary<RTCVideoCodecInfo *, id> *_dictionary;
  NSMutableArray<RTCVideoCodecInfo *> *_formatsByPriority;
}

- (instancetype)init {
  if (self = [super init]) {
    _dictionary = [NSMutableDictionary dictionary];
    _formatsByPriority = [NSMutableArray array];
  }

  return self;
}

- (void)addCodecInfo:(RTCVideoCodecInfo *)codecInfo withClass:(id)codecClass {
  if (codecInfo && codecClass) {
    [_dictionary setObject:codecClass forKey:codecInfo];
    [_formatsByPriority addObject:codecInfo];
  }
}

- (NSArray<RTCVideoCodecInfo *> *)codecsByPriority {
  return [_formatsByPriority copy];
}

- (id)codecClassForCodecInfo:(RTCVideoCodecInfo *)codecInfo {
  return [_dictionary objectForKey:codecInfo];
}

@end

#pragma mark - Encoder Factory

@implementation RTCDefaultVideoEncoderFactory {
  RTCVideoCodecPriorityList<Class<RTCVideoEncoder>> *_codecList;
}

- (instancetype)initWithEncoderPriorityList:
        (RTCVideoCodecPriorityList<Class<RTCVideoEncoder>> *)codecList {
  if (self = [super init]) {
    _codecList = codecList;
  }

  return self;
}

- (id<RTCVideoEncoder>)createEncoder:(RTCVideoCodecInfo *)info {
  Class<RTCVideoEncoder> encoderClass = [_codecList codecClassForCodecInfo:info];
  id<RTCVideoEncoder> encoderInstance = [[(Class)encoderClass alloc] init];
  if ([encoderInstance respondsToSelector:@selector(configureWithCodecInfo:)]) {
    [encoderInstance configureWithCodecInfo:info];
  }
  return encoderInstance;
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  return [_codecList codecsByPriority];
}

@end

#pragma mark - Decoder Factory

@implementation RTCDefaultVideoDecoderFactory {
  RTCVideoCodecPriorityList<Class<RTCVideoDecoder>> *_codecList;
}

- (instancetype)initWithDecoderPriorityList:
        (RTCVideoCodecPriorityList<Class<RTCVideoDecoder>> *)codecList {
  if (self = [super init]) {
    _codecList = codecList;
  }

  return self;
}

- (id<RTCVideoDecoder>)createDecoder:(RTCVideoCodecInfo *)info {
  Class<RTCVideoDecoder> decoderClass = [_codecList codecClassForCodecInfo:info];
  id<RTCVideoDecoder> decoderInstance = [[(Class)decoderClass alloc] init];
  if ([decoderInstance respondsToSelector:@selector(configureWithCodecInfo:)]) {
    [decoderInstance configureWithCodecInfo:info];
  }
  return decoderInstance;
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  return [_codecList codecsByPriority];
}

@end
