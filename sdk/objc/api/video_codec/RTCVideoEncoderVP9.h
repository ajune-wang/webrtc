/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#include <memory>

#import "RTCMacros.h"
#import "RTCNativeVideoEncoder.h"
#import "RTCNativeVideoEncoderBuilder+Native.h"

#include "api/environment/environment.h"
#include "api/video_codecs/video_encoder.h"

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCVideoEncoderVP9)
    : RTC_OBJC_TYPE(RTCNativeVideoEncoder) <RTC_OBJC_TYPE (RTCNativeVideoEncoderBuilder)>

/* This returns a VP9 encoder that can be returned from a RTCVideoEncoderFactory injected into
 * RTCPeerConnectionFactory. Even though it implements the RTCVideoEncoder protocol, it can not be
 * used independently from the RTCPeerConnectionFactory.
 */
+ (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)vp9Encoder;

+ (bool)isSupported;

- (std::unique_ptr<webrtc::VideoEncoder>)build:(const webrtc::Environment &)env;

@end
