/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <VideoToolbox/VideoToolbox.h>

#import "RTCMacros.h"
#import "RTCVideoDecoder.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"

void decompressionOutputCallback(void *decoderRef,
                                 void *params,
                                 OSStatus status,
                                 VTDecodeInfoFlags infoFlags,
                                 CVImageBufferRef imageBuffer,
                                 CMTime timestamp,
                                 CMTime duration);

// RTCVideoToolboxDecoder is a abstract class for implementing a VideoToolbox Decoder.
// When developing a decoder using VideoToolbox, inherit from RTCVideoToolboxDecoder.
RTC_OBJC_EXPORT
@protocol RTC_OBJC_TYPE
(RTCVideoToolboxDecoder)<RTCVideoDecoder>

    - (void)setError : (OSStatus)error;

@end

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCVideoToolboxDecoder) : NSObject <RTCVideoToolboxDecoder>
@end
