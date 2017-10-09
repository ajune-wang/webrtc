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

#pragma mark - Encoder Factory

@implementation RTCVideoEncoderPriorityList {
  NSMutableDictionary<RTCVideoCodecInfo *, Class<RTCVideoEncoder>> *_dictionary;
  NSMutableArray<RTCVideoCodecInfo *> *_formatsByPriority;
}

- (instancetype)init {
  if (self = [super init]) {
    _dictionary = [NSMutableDictionary dictionary];
    _formatsByPriority = [NSMutableArray array];
  }

  return self;
}

- (void)addFormat:(RTCVideoCodecInfo *)codecInfo withClass:(Class<RTCVideoEncoder>)encoderClass {
  [_dictionary setObject:encoderClass forKey:codecInfo];
  [_formatsByPriority addObject:codecInfo];
}

- (NSArray<RTCVideoCodecInfo *> *)formatsByPriority {
  return [_formatsByPriority copy];
}

- (Class<RTCVideoEncoder>)encoderForFormat:(RTCVideoCodecInfo *)codecInfo {
  return [_dictionary objectForKey:codecInfo];
}

@end

@implementation RTCDefaultVideoEncoderFactory {
  RTCVideoEncoderPriorityList *_codecList;
}

- (instancetype)initWithEncoderPriorityList:(RTCVideoEncoderPriorityList *)codecList {
  if (self = [super init]) {
    _codecList = codecList;
  }

  return self;
}

- (id<RTCVideoEncoder>)createEncoder:(RTCVideoCodecInfo *)info {
  Class<RTCVideoEncoder> encoderClass = [_codecList encoderForFormat:info];
  return [[(Class)encoderClass alloc] initWithCodecInfo:info];
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  return [_codecList formatsByPriority];
}

@end

#pragma mark - Decoder Factory

@implementation RTCVideoDecoderPriorityList {
  NSMutableDictionary<RTCVideoCodecInfo *, Class<RTCVideoDecoder>> *_dictionary;
  NSMutableArray<RTCVideoCodecInfo *> *_formatsByPriority;
}

- (instancetype)init {
  if (self = [super init]) {
    _dictionary = [NSMutableDictionary dictionary];
    _formatsByPriority = [NSMutableArray array];
  }

  return self;
}

- (void)addFormat:(RTCVideoCodecInfo *)codecInfo withClass:(Class<RTCVideoDecoder>)decoderClass {
  [_dictionary setObject:decoderClass forKey:codecInfo];
  [_formatsByPriority addObject:codecInfo];
}

- (NSArray<RTCVideoCodecInfo *> *)formatsByPriority {
  return [_formatsByPriority copy];
}

- (Class<RTCVideoDecoder>)decoderForFormat:(RTCVideoCodecInfo *)codecInfo {
  return [_dictionary objectForKey:codecInfo];
}

@end

@implementation RTCDefaultVideoDecoderFactory {
  RTCVideoDecoderPriorityList *_codecList;
}

- (instancetype)initWithDecoderPriorityList:(RTCVideoDecoderPriorityList *)codecList {
  if (self = [super init]) {
    _codecList = codecList;
  }

  return self;
}

- (id<RTCVideoDecoder>)createDecoder:(RTCVideoCodecInfo *)info {
  Class<RTCVideoDecoder> decoderClass = [_codecList decoderForFormat:info];
  return [[(Class)decoderClass alloc] initWithCodecInfo:info];
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  return [_codecList formatsByPriority];
}

@end
