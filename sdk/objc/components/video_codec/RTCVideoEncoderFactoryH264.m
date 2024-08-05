/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoEncoderFactoryH264.h"

#import "RTCH264ProfileLevelId.h"
#import "RTCVideoEncoderH264.h"

@implementation RTC_OBJC_TYPE (RTCVideoEncoderFactoryH264)

- (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)supportedCodecs {
  NSMutableArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *codecs = [NSMutableArray array];
  NSString *codecName = kRTCVideoCodecH264Name;

  NSDictionary<NSString *, NSString *> *constrainedHighParams = @{
    @"profile-level-id" : kRTCMaxSupportedH264ProfileLevelConstrainedHigh,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTC_OBJC_TYPE(RTCVideoCodecInfo) *constrainedHighInfo =
      [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:codecName
                                                  parameters:constrainedHighParams];
  [codecs addObject:constrainedHighInfo];

  NSDictionary<NSString *, NSString *> *constrainedBaselineParams = @{
    @"profile-level-id" : kRTCMaxSupportedH264ProfileLevelConstrainedBaseline,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTC_OBJC_TYPE(RTCVideoCodecInfo) *constrainedBaselineInfo =
      [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:codecName
                                                  parameters:constrainedBaselineParams];
  [codecs addObject:constrainedBaselineInfo];

  return [codecs copy];
}

- (nonnull RTC_OBJC_TYPE(RTCVideoEncoderCodecSupport) *)
    queryCodecSupport:(nonnull RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info
      scalabilityMode:(nullable NSString *)scalabilityMode {
  if (scalabilityMode != nil && ![scalabilityMode isEqualToString:@"L1T1"]) {
    // Only nonscalable video is supported.
    return [[RTC_OBJC_TYPE(RTCVideoEncoderCodecSupport) alloc] initIsSupported:false];
  }
  for (RTC_OBJC_TYPE(RTCVideoCodecInfo) * supportedCodec in [self supportedCodecs]) {
    if ([supportedCodec isEqualToCodecInfo:info]) {
      return [[RTC_OBJC_TYPE(RTCVideoEncoderCodecSupport) alloc] initIsSupported:true];
    }
  }
  return [[RTC_OBJC_TYPE(RTCVideoEncoderCodecSupport) alloc] initIsSupported:false];
}

- (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)createEncoder:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info {
  return [[RTC_OBJC_TYPE(RTCVideoEncoderH264) alloc] initWithCodecInfo:info];
}

@end
