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

#import "RTCMacros.h"
#import "RTCNativeVideoEncoder.h"
#import "RTCNativeVideoEncoderBuilder+Native.h"
#import "RTCVideoEncoderVP8.h"
#import "helpers/NSString+StdString.h"

#include "absl/types/optional.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/scalability_mode_helper.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp8/vp8_scalability.h"

@interface RTC_OBJC_TYPE (RTCVideoEncoderVP8Builder)
    : RTC_OBJC_TYPE(RTCNativeVideoEncoder) <RTC_OBJC_TYPE (RTCNativeVideoEncoderBuilder)>
@end

    @implementation RTC_OBJC_TYPE (RTCVideoEncoderVP8Builder)

    - (std::unique_ptr<webrtc::VideoEncoder>)build:(const webrtc::Environment&)env {
      return webrtc::CreateVp8Encoder(env);
    }

    @end

    @implementation RTC_OBJC_TYPE (RTCVideoEncoderVP8)

    + (bool)isScalabilityModeSupported:(nonnull NSString*)name {
      absl::optional<webrtc::ScalabilityMode> mode =
          webrtc::ScalabilityModeStringToEnum([NSString stdStringForString:name]);
      return mode.has_value() && webrtc::VP8SupportsScalabilityMode(*mode);
    }

    + (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)vp8Encoder {
      return [[RTC_OBJC_TYPE(RTCVideoEncoderVP8Builder) alloc] init];
    }

    @end
