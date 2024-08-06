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

#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCNativeVideoEncoder.h"
#import "RTCNativeVideoEncoderBuilder+Native.h"
#import "RTCVideoEncoderAV1.h"
#import "helpers/NSString+StdString.h"

#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/scalability_mode_helper.h"
#include "modules/video_coding/codecs/av1/av1_svc_config.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"

@interface RTC_OBJC_TYPE (RTCVideoEncoderAV1Builder)
    : RTC_OBJC_TYPE(RTCNativeVideoEncoder) <RTC_OBJC_TYPE (RTCNativeVideoEncoderBuilder)>
@end

    @implementation RTC_OBJC_TYPE (RTCVideoEncoderAV1Builder)

    - (std::unique_ptr<webrtc::VideoEncoder>)build:(const webrtc::Environment&)env {
      return webrtc::CreateLibaomAv1Encoder(env);
    }

    @end

    @implementation RTC_OBJC_TYPE (RTCVideoEncoderAV1)

    + (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)av1Encoder {
      return [[RTC_OBJC_TYPE(RTCVideoEncoderAV1Builder) alloc] init];
    }

    + (bool)isScalabilityModeSupported:(nonnull NSString*)name {
      absl::optional<webrtc::ScalabilityMode> mode =
          webrtc::ScalabilityModeStringToEnum([NSString stdStringForString:name]);
      if (!mode.has_value()) {
        return false;
      }
      return webrtc::LibaomAv1EncoderSupportsScalabilityMode(*mode);
    }

    + (bool)isSupported {
      return true;
    }

    @end
