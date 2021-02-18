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

#import "base/RTCVideoEncoder.h"
#import "base/RTCVideoEncoderFactory.h"

@interface RTC_OBJC_TYPE (RTCFakeVideoEncoder) : NSObject <RTC_OBJC_TYPE(RTCVideoEncoder)>
@end

@interface RTC_OBJC_TYPE (RTCFakeVideoEncoderFactory) : NSObject <RTC_OBJC_TYPE(RTCVideoEncoderFactory)>
@end
