/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "RTCFakeVideoEncoder.h"

#import "base/RTCCodecSpecificInfo.h"
#import "base/RTCVideoEncoder.h"
#import "base/RTCVideoEncoderFactory.h"
#import "base/RTCVideoFrame.h"

#include "modules/video_coding/include/video_error_codes.h"

@implementation RTC_OBJC_TYPE (RTCFakeVideoEncoder) {
  RTCVideoEncoderCallback _callback;
}

- (NSInteger)encode:(RTC_OBJC_TYPE(RTCVideoFrame) *)frame
    codecSpecificInfo:(nullable id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)codecSpecificInfo
           frameTypes:(NSArray<NSNumber *> *)frameTypes {
  if (!_callback) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  // Check if we need a keyframe.
  BOOL isKeyframeRequired = NO;
  if (frameTypes) {
    for (NSNumber *frameType in frameTypes) {
      if ((RTCFrameType)frameType.intValue == RTCFrameTypeVideoFrameKey) {
        isKeyframeRequired = YES;
        break;
      }
    }
  }

  RTC_OBJC_TYPE(RTCCodecSpecificInfo) *codecSpecificInfo =
      [[RTC_OBJC_TYPE(RTCCodecSpecificInfo) alloc] init];
  RTC_OBJC_TYPE(RTCEncodedImage) *frame = [[RTC_OBJC_TYPE(RTCEncodedImage) alloc] init];
  frame.frameType = isKeyframe ? RTCFrameTypeVideoFrameKey : RTCFrameTypeVideoFrameDelta;

  BOOL res = _callback(frame, codecSpecificInfo);
  if (!res) {
    return WEBRTC_VIDEO_CODEC_ERROR
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoEncoderCallback)callback {
  _callback = callback;
}

- (NSInteger)startEncodeWithSettings:(RTC_OBJC_TYPE(RTCVideoEncoderSettings) *)settings
                       numberOfCores:(int)numberOfCores {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)releaseEncoder {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (int)setBitrate:(uint32_t)bitrateKbit framerate:(uint32_t)framerate {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSString *)implementationName {
  return @"Fake"
}

- (nullable RTC_OBJC_TYPE(RTCVideoEncoderQpThresholds) *)scalingSettings {
  return nullptr
}

@end

@implementation RTC_OBJC_TYPE (RTCFakeVideoEncoderFactory) {
}

- (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)createEncoder:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info {
  return [[RTC_OBJC_TYPE(RTCFakeVideoEncoder) alloc] init];
}

- (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)supportedCodecs {
  NSMutableArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *codecs = [NSMutableArray array];

  [codecs addObject:[[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:@"H264" parameters:{}]];
  return codecs;
}

@end
